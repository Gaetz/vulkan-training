# Lesson 2 : Better resource management with Vulkan

In this lesson, we wil improve resource management to make it easier to deal with materials that might have a varying number of textures. It is often referred to a bindless, even if it is not accurae. We are still goind to bind resources, but using an index rather thn having to specify exactly which resource are going to be used during a draw.

We will also automate the creation of pipeline layouts. Most project have a lot of shaders, and we need to automate the way they are handled by the pipeline, using the information provided by the SIR-V binary format.

Finally, we'll add pipeline caching to our graphics engine. This will improve the creation time of pipeline after the first run, thus speeding up loading time.

## Implemeting bindless rendering

### Checking for support

To support bindless rendering, we need to check the indexing extension is available. Add this code above the `// Create logical device` commentary:

`gpu_device.cpp`
```
void GpuDevice::init( const DeviceCreation& creation ) {
...
// [TAG: BINDLESS]
// Query bindless extension, called Descriptor Indexing
// (https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VK_EXT_descriptor_indexing.html)
VkPhysicalDeviceDescriptorIndexingFeatures indexing_features{
  VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES, nullptr };
VkPhysicalDeviceFeatures2 device_features {
  VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &indexing_features };

vkGetPhysicalDeviceFeatures2( vulkan_physical_device, &device_features );
// For the feature to be correctly working, we need both the possibility
// to partially bind a descriptor, as some entries in the bindless array
// will be empty, and SpirV runtime descriptors.
bindless_supported = indexing_features.descriptorBindingPartiallyBound
  && indexing_features.runtimeDescriptorArray;

//////// Create logical device
...

}
```
Once we have confirmed that the extension is supported, we can enable it when creating the device. We have to chain the indexing_features variable to the physical_features2 variable used when creating our device:

`gpu_device.cpp`
```
VkDeviceCreateInfo device_create_info = {};
...

// [TAG: BINDLESS]
// We also add the bindless needed feature on the device creation.
if ( bindless_supported ) {
    indexing_features.descriptorBindingPartiallyBound = VK_TRUE;
    indexing_features.runtimeDescriptorArray = VK_TRUE;

    physical_features2.pNext = &indexing_features;
}

result = vkCreateDevice( vulkan_physical_device, &device_create_info,
  vulkan_allocation_callbacks, &vulkan_device );
...
```

### Creating the descriptor pool

The next step is to create a descriptor pool from which we can allocate descriptor sets that support updating the content of a texture after it is bound.

First define variables and constants we need.

`gpu_device.hpp`
```
  ...
  VkDescriptorPool vulkan_descriptor_pool;

  // [TAG: BINDLESS]

  VkDescriptorPool vulkan_bindless_descriptor_pool;
  // Global bindless descriptor layout.
  VkDescriptorSetLayout vulkan_bindless_descriptor_layout;
  // Global bindless descriptor set.
  VkDescriptorSet vulkan_bindless_descriptor_set;
  ...
  // These are dynamic - so that workload can be handled correctly.
  ...
  // [TAG: BINDLESS]
  Array<ResourceUpdate> texture_to_update_bindless;
```

We need to initialize new constants and allocate memory for `texture_to_update_bindless`:

`gpu_device.cpp`
```
static const u32 k_bindless_texture_binding = 10;
static const u32 k_max_bindless_resources = 1024;
...
void GpuDevice::init( const DeviceCreation& creation ) {
  ...
  texture_to_update_bindless.init( allocator, 16 );

  //
  // Init primitive resources
  //
  ...
}
```

Then update the code to support bindless descriptor pool.

`gpu_device.cpp`
```
////////  Create Pools
...
check( result );

// [TAG: BINDLESS]
// Create the Descriptor Pool used by bindless, that needs update after bind flag.
if ( bindless_supported ) {
  VkDescriptorPoolSize pool_sizes_bindless[] =
  {
    { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, k_max_bindless_resources },
    { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, k_max_bindless_resources },
  };

  // Update after bind is needed here, for each binding
  // and in the descriptor set layout creation.
  pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT;
  pool_info.maxSets = k_max_bindless_resources * ArraySize( pool_sizes_bindless );
  pool_info.poolSizeCount = ( u32 )ArraySize( pool_sizes_bindless );
  pool_info.pPoolSizes = pool_sizes_bindless;
  result = vkCreateDescriptorPool(
    vulkan_device, &pool_info, vulkan_allocation_callbacks,
    &vulkan_bindless_descriptor_pool
  );
  check( result );

  const u32 pool_count = ( u32 )ArraySize( pool_sizes_bindless );
  VkDescriptorSetLayoutBinding vk_binding[ 4 ];

  // Actual descriptor set layout
  VkDescriptorSetLayoutBinding& image_sampler_binding = vk_binding[ 0 ];
  image_sampler_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  image_sampler_binding.descriptorCount = k_max_bindless_resources;
  image_sampler_binding.binding = k_bindless_texture_binding;
  image_sampler_binding.stageFlags = VK_SHADER_STAGE_ALL;
  image_sampler_binding.pImmutableSamplers = nullptr;

  VkDescriptorSetLayoutBinding& storage_image_binding = vk_binding[ 1 ];
  storage_image_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  storage_image_binding.descriptorCount = k_max_bindless_resources;
  storage_image_binding.binding = k_bindless_texture_binding + 1;
  storage_image_binding.stageFlags = VK_SHADER_STAGE_ALL;
  storage_image_binding.pImmutableSamplers = nullptr;
  ...
```

Notice that descriptorCount no longer has a value of 1 but has to accommodate the maximum number of textures we can use. We can now use this data to create a descriptor set layout:

```
...
  VkDescriptorSetLayoutCreateInfo layout_info = {
    VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
  layout_info.bindingCount = pool_count;
  layout_info.pBindings = vk_binding;
  layout_info.flags =
    VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT;

  // Binding flags
  VkDescriptorBindingFlags bindless_flags =
    VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT
    | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT;
  VkDescriptorBindingFlags binding_flags[ 4 ];

  binding_flags[ 0 ] = bindless_flags;
  binding_flags[ 1 ] = bindless_flags;

  VkDescriptorSetLayoutBindingFlagsCreateInfoEXT extended_info{
    VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT,
    nullptr
  };
  extended_info.bindingCount = pool_count;
  extended_info.pBindingFlags = binding_flags;

  layout_info.pNext = &extended_info;

  vkCreateDescriptorSetLayout(
    vulkan_device, &layout_info, vulkan_allocation_callbacks,
    &vulkan_bindless_descriptor_layout
  );
...
```
We have added the bindless_flags values to enable partial updates to the descriptor set. We also have to chain a `VkDescriptorSetLayoutBindingFlagsCreateInfoEXT` structure to the layout_info variable. Finally, we can create the descriptor set we are going to use for the lifetime of the application:

```
...
  VkDescriptorSetAllocateInfo alloc_info{
    VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
  alloc_info.descriptorPool = vulkan_bindless_descriptor_pool;
  alloc_info.descriptorSetCount = 1;
  alloc_info.pSetLayouts = &vulkan_bindless_descriptor_layout;

  VkDescriptorSetVariableDescriptorCountAllocateInfoEXT count_info{
    VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT };
  u32 max_binding = k_max_bindless_resources - 1;
  count_info.descriptorSetCount = 1;
  // This number is the max allocatable count
  count_info.pDescriptorCounts = &max_binding;

  check_result( vkAllocateDescriptorSets(
    vulkan_device, &alloc_info, &vulkan_bindless_descriptor_set
  ) );
}
```

### Updating the descriptor set

Now, when we call `vulkan_create_texture`, the newly created resource gets added to the `texture_to_update_bindless` array:

```
static void vulkan_create_texture(
  GpuDevice& gpu, const TextureCreation& creation,
  TextureHandle handle, Texture* texture
) {
  ...
  // Add deferred bindless update.
  if ( gpu.bindless_supported ) {
    ResourceUpdate resource_update{
      ResourceDeletionType::Texture, texture->handle.index, gpu.current_frame
    };
    gpu.texture_to_update_bindless.push( resource_update );
  }
}
```

In the `main.cpp` code, it will be possible to associate a specific sampler to a given texture, with this code. We will see that in a moment.
```
gpu.link_texture_sampler( diffuse_texture_gpu.handle,
 diffuse_sampler_gpu.handle );
```

This links the diffuse texture with its sampler. This information will be used in the next code section to determine whether we use a default sampler or the one we have just assigned to the texture. Before the next frame is processed, we update the descriptor set we have created in the previous section with any new textures that have been uploaded:

```
void GpuDevice::present() {
...
// Copy all commands
VkCommandBuffer enqueued_command_buffers[ 4 ];
for ( u32 c = 0; c < num_queued_command_buffers; c++ ) {
    ...
}

if (texture_to_update_bindless.size) {
  // Handle deferred writes to bindless textures.
  VkWriteDescriptorSet bindless_descriptor_writes[k_max_bindless_resources];
  VkDescriptorImageInfo bindless_image_info[k_max_bindless_resources];

  Texture* vk_dummy_texture = access_texture(dummy_texture);

  u32 current_write_index = 0;
  for (i32 it = texture_to_update_bindless.size - 1; it >= 0; it--) {
    ResourceUpdate& texture_to_update = texture_to_update_bindless[it];

    //if ( texture_to_update.current_frame == current_frame )
    {
      Texture* texture = access_texture({ texture_to_update.handle });
      VkWriteDescriptorSet& descriptor_write =
        bindless_descriptor_writes[current_write_index];
      descriptor_write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
      descriptor_write.descriptorCount = 1;
      descriptor_write.dstArrayElement = texture_to_update.handle;
      descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      descriptor_write.dstSet = vulkan_bindless_descriptor_set;
      descriptor_write.dstBinding = k_bindless_texture_binding;

      // Handles should be the same.
      RASSERT(texture->handle.index == texture_to_update.handle);

      Sampler* vk_default_sampler = access_sampler(default_sampler);
      VkDescriptorImageInfo& descriptor_image_info = bindless_image_info[current_write_index];

      if (texture->sampler != nullptr) {
          descriptor_image_info.sampler = texture->sampler->vk_sampler;
      }
      else {
          descriptor_image_info.sampler = vk_default_sampler->vk_sampler;
      }

      descriptor_image_info.imageView = texture->vk_format != VK_FORMAT_UNDEFINED ? texture->vk_image_view : vk_dummy_texture->vk_image_view;
      descriptor_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      descriptor_write.pImageInfo = &descriptor_image_info;

      texture_to_update.current_frame = u32_max;

      texture_to_update_bindless.delete_swap(it);

      ++current_write_index;
    }
  }

  if (current_write_index) {
    vkUpdateDescriptorSets(vulkan_device, current_write_index, bindless_descriptor_writes, 0, nullptr);
  }
}

// Submit command buffers
...
```

When creating or filling descriptor sets, we do not need to bind images and textures anymore:
```
DescriptorSetLayoutHandle GpuDevice::create_descriptor_set_layout(
  const DescriptorSetLayoutCreation& creation ) {
  ...
  for ( u32 r = 0; r < creation.num_bindings; ++r ) {
    DescriptorBinding& binding = descriptor_set_layout->bindings[ r ];
    const DescriptorSetLayoutCreation::Binding& input_binding = creation.bindings[ r ];
    binding.start = input_binding.start == u16_max ?
      ( u16 )r : input_binding.start;
    binding.count = 1;
    binding.type = input_binding.type;
    binding.name = input_binding.name;

    // [TAG: BINDLESS]
    // Skip bindings for images and textures as they are bindless, thus bound in the global bindless arrays (one for images, one for textures).
    if ( bindless_supported && (binding.type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER || binding.type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) ) {
        continue;
    }
    ...
  }
  ...
}

void GpuDevice::fill_write_descriptor_sets( GpuDevice& gpu,
  const DesciptorSetLayout* descriptor_set_layout, VkDescriptorSet vk_descriptor_set,
  VkWriteDescriptorSet* descriptor_write, VkDescriptorBufferInfo* buffer_info,
  VkDescriptorImageInfo* image_info, VkSampler vk_default_sampler,
  u32& num_resources, const ResourceHandle* resources,
  const SamplerHandle* samplers, const u16* bindings ) {

  u32 used_resources = 0;
  for ( u32 r = 0; r < num_resources; r++ ) {
    // Binding array contains the index into the resource layout binding to retrieve
    // the correct binding informations.
    u32 layout_binding_index = bindings[ r ];

    const DescriptorBinding& binding =
      descriptor_set_layout->bindings[ layout_binding_index ];

    // [TAG: BINDLESS]
    // Skip bindless descriptors as they are bound in the global bindless arrays.
    if ( gpu.bindless_supported &&
      (binding.type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
      || binding.type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) ) {
        continue;
    }
    ...
  }
  ...
}

```

When creating the pipeline, we must link the bindless descriptor layout:
```
...
u32 num_active_layouts = creation.num_active_layouts;

// Create VkPipelineLayout
for ( u32 l = 0; l < shader_state_data->parse_result->set_count; ++l ) {
  ...
}

// Add bindless resource layout after other layouts.
// [TAG: BINDLESS]
u32 bindless_active = 0;
if ( bindless_supported ) {
  vk_layouts[ num_active_layouts ] = vulkan_bindless_descriptor_layout;
  bindless_active = 1;
}

VkPipelineLayoutCreateInfo pipeline_layout_info = {
  VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
pipeline_layout_info.pSetLayouts = vk_layouts;
pipeline_layout_info.setLayoutCount = num_active_layouts + bindless_active;

VkPipelineLayout pipeline_layout;
check( vkCreatePipelineLayout( vulkan_device, &pipeline_layout_info,
  vulkan_allocation_callbacks, &pipeline_layout ) );
// Cache pipeline layout
pipeline->vk_pipeline_layout = pipeline_layout;
pipeline->num_active_layouts = num_active_layouts;
...
```

### Updating the command buffer

First we need to reset the descriptor pool and descriptor sets when reseting the command buffer.

`command_buffer.cpp`
```
void CommandBuffer::reset() {

    is_recording = false;
    current_render_pass = nullptr;
    current_pipeline = nullptr;
    current_command = 0;

    vkResetDescriptorPool( gpu_device->vulkan_device, vk_descriptor_pool, 0 );

    u32 resource_count = descriptor_sets.free_indices_head;
    for ( u32 i = 0; i < resource_count; ++i) {
        DesciptorSet* v_descriptor_set = ( DesciptorSet* )descriptor_sets.access_resource( i );

        if ( v_descriptor_set ) {
            // Contains the allocation for all the resources, binding and samplers arrays.
            rfree( v_descriptor_set->resources, gpu_device->allocator );
        }
        descriptor_sets.release_resource( i );
    }
}
```

Then when binding descriptor sets, add support for bindless descriptor set, that will just take the start of the data:

```
void CommandBuffer::bind_descriptor_set( DescriptorSetHandle* handles, u32 num_lists, u32* offsets, u32 num_offsets ) {
  ...
  if ( gpu_device->bindless_supported ) {
    vkCmdBindDescriptorSets( vk_command_buffer,
    current_pipeline->vk_bind_point, current_pipeline->vk_pipeline_layout,
    1, 1, &gpu_device->vulkan_bindless_descriptor_set, 0, nullptr );
  }
}

void CommandBuffer::bind_local_descriptor_set( DescriptorSetHandle* handles, u32 num_lists, u32* offsets, u32 num_offsets ) {
  ...
  if ( gpu_device->bindless_supported ) {
    vkCmdBindDescriptorSets( vk_command_buffer,
    current_pipeline->vk_bind_point, current_pipeline->vk_pipeline_layout,
    1, 1, &gpu_device->vulkan_bindless_descriptor_set, 0, nullptr );
  }
}
```

### Cleaning

Do not forget to destroy or shut our new variable at exit time:
```
void GpuDevice::shutdown() {
  ...
  vmaDestroyAllocator( vma_allocator );

  texture_to_update_bindless.shutdown();
  ...
    // [TAG: BINDLESS]
  if ( bindless_supported ) {
    vkDestroyDescriptorSetLayout( vulkan_device,
      vulkan_bindless_descriptor_layout, vulkan_allocation_callbacks );
    vkDestroyDescriptorPool( vulkan_device,
      vulkan_bindless_descriptor_pool, vulkan_allocation_callbacks );
  }

  vkDestroyDescriptorPool( vulkan_device,
    vulkan_descriptor_pool, vulkan_allocation_callbacks );
  ...
}

void GpuDevice::destroy_texture( TextureHandle texture ) {
  if ( texture.index < textures.pool_size ) {
    resource_deletion_queue.push(
      { ResourceDeletionType::Texture, texture.index, current_frame } );
    texture_to_update_bindless.push(
      { ResourceDeletionType::Texture, texture.index, current_frame } );
  } else {
    rprint( "Graphics error: trying to free invalid Texture %u\n", texture.index );
  }
}
```

### Update to shader code

The final piece of the puzzle to use bindless rendering is in the shader code, as it needs to be written in a different way.

We will now write the shaders in standalone files. Here is the complete code. It will broken down just below.

`main.vert`
```
#version 450

layout ( std140, binding = 0 ) uniform LocalConstants {
    mat4        view_projection;
    vec4        eye;
    vec4        light;
    float       light_range;
    float       light_intensity;
};

layout ( std140, binding = 1 ) uniform Mesh {

    mat4        model;
    mat4        model_inverse;

    // x = diffuse index, y = roughness index, z = normal index, w = occlusion index.
    // Occlusion and roughness are encoded in the same texture
    uvec4       textures;
    vec4        base_color_factor;
    vec4        metallic_roughness_occlusion_factor;
    float       alpha_cutoff;
    uint        flags;
};

layout(location=0) in vec3 position;
layout(location=1) in vec4 tangent;
layout(location=2) in vec3 normal;
layout(location=3) in vec2 texCoord0;

layout (location = 0) out vec2 vTexcoord0;
layout (location = 1) out vec3 vNormal;
layout (location = 2) out vec3 vTangent;
layout (location = 3) out vec3 vBiTangent;
layout (location = 4) out vec3 vPosition;

void main() {
    vec4 worldPosition = model * vec4(position, 1.0);
    gl_Position = view_projection * worldPosition;
    vPosition = worldPosition.xyz / worldPosition.w;
    vTexcoord0 = texCoord0;
    vNormal = normalize( mat3(model_inverse) * normal );
    vTangent = normalize( mat3(model) * tangent.xyz );
    vBiTangent = cross( vNormal, vTangent ) * tangent.w;
}
```

`main.frag`
```
#version 450

layout ( std140, binding = 0 ) uniform LocalConstants {
    mat4        view_projection;
    vec4        eye;
    vec4        light;
    float       light_range;
    float       light_intensity;
};

uint DrawFlags_AlphaMask = 1 << 0;

layout ( std140, binding = 1 ) uniform Mesh {

    mat4        model;
    mat4        model_inverse;

    // x = diffuse index, y = roughness index, z = normal index, w = occlusion index.
    // Occlusion and roughness are encoded in the same texture
    uvec4       textures;
    vec4        base_color_factor;
    vec4        metallic_roughness_occlusion_factor;
    float       alpha_cutoff;
    uint        flags;
};

// Bindless support
// Enable non uniform qualifier extension
#extension GL_EXT_nonuniform_qualifier : enable
// Global bindless support. This should go in a common file.

layout ( set = 1, binding = 10 ) uniform sampler2D global_textures[];
// Alias textures to use the same binding point, as bindless texture is shared
// between all kind of textures: 1d, 2d, 3d.
layout ( set = 1, binding = 10 ) uniform sampler3D global_textures_3d[];


layout (location = 0) in vec2 vTexcoord0;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec3 vTangent;
layout (location = 3) in vec3 vBiTangent;
layout (location = 4) in vec3 vPosition;

layout (location = 0) out vec4 frag_color;

#define PI 3.1415926538
#define INVALID_TEXTURE_INDEX 65535

vec3 decode_srgb( vec3 c ) {
    vec3 result;
    if ( c.r <= 0.04045) {
        result.r = c.r / 12.92;
    } else {
        result.r = pow( ( c.r + 0.055 ) / 1.055, 2.4 );
    }

    if ( c.g <= 0.04045) {
        result.g = c.g / 12.92;
    } else {
        result.g = pow( ( c.g + 0.055 ) / 1.055, 2.4 );
    }

    if ( c.b <= 0.04045) {
        result.b = c.b / 12.92;
    } else {
        result.b = pow( ( c.b + 0.055 ) / 1.055, 2.4 );
    }

    return clamp( result, 0.0, 1.0 );
}

vec3 encode_srgb( vec3 c ) {
    vec3 result;
    if ( c.r <= 0.0031308) {
        result.r = c.r * 12.92;
    } else {
        result.r = 1.055 * pow( c.r, 1.0 / 2.4 ) - 0.055;
    }

    if ( c.g <= 0.0031308) {
        result.g = c.g * 12.92;
    } else {
        result.g = 1.055 * pow( c.g, 1.0 / 2.4 ) - 0.055;
    }

    if ( c.b <= 0.0031308) {
        result.b = c.b * 12.92;
    } else {
        result.b = 1.055 * pow( c.b, 1.0 / 2.4 ) - 0.055;
    }

    return clamp( result, 0.0, 1.0 );
}

float heaviside( float v ) {
    if ( v > 0.0 ) return 1.0;
    else return 0.0;
}

void main() {
    vec4 base_colour = texture(global_textures[nonuniformEXT(textures.x)], vTexcoord0) * base_color_factor;

    bool useAlphaMask = (flags & DrawFlags_AlphaMask) != 0;
    if (useAlphaMask && base_colour.a < alpha_cutoff) {
        base_colour.a = 0.0;
    }

    vec3 normal = normalize( vNormal );
    vec3 tangent = normalize( vTangent );
    vec3 bitangent = normalize( vBiTangent );

    if (gl_FrontFacing == false)
    {
        tangent *= -1.0;
        bitangent *= -1.0;
        normal *= -1.0;
    }

    if (textures.z != INVALID_TEXTURE_INDEX) {
        // NOTE(marco): normal textures are encoded to [0, 1] but need to be mapped to [-1, 1] value
        vec3 bump_normal = normalize( texture(global_textures[nonuniformEXT(textures.z)], vTexcoord0).rgb * 2.0 - 1.0 );
        mat3 TBN = mat3(
            tangent,
            bitangent,
            normal
        );

        normal = normalize(TBN * normalize(bump_normal));
    }

    vec3 V = normalize( eye.xyz - vPosition );
    vec3 L = normalize( light.xyz - vPosition );
    vec3 N = normal;
    vec3 H = normalize( L + V );

    float metalness = metallic_roughness_occlusion_factor.x;
    float roughness = metallic_roughness_occlusion_factor.y;

    if (textures.w != INVALID_TEXTURE_INDEX) {
        vec4 rm = texture(global_textures[nonuniformEXT(textures.y)], vTexcoord0);

        // Green channel contains roughness values
        roughness *= rm.g;

        // Blue channel contains metalness
        metalness *= rm.b;
    }

    float alpha = pow(roughness, 2.0);

    float occlusion = metallic_roughness_occlusion_factor.z;
    if (textures.w != INVALID_TEXTURE_INDEX) {
        vec4 o = texture(global_textures[nonuniformEXT(textures.w)], vTexcoord0);
        // Red channel for occlusion value
        occlusion *= o.r;
    }

    base_colour.rgb = decode_srgb( base_colour.rgb );

    // https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html#specular-brdf
    float NdotH = clamp(dot(N, H), 0, 1);
    float alpha_squared = alpha * alpha;
    float d_denom = ( NdotH * NdotH ) * ( alpha_squared - 1.0 ) + 1.0;
    float distribution = ( alpha_squared * heaviside( NdotH ) ) / ( PI * d_denom * d_denom );

    float NdotL = clamp(dot(N, L), 0, 1);
    float NdotV = clamp(dot(N, V), 0, 1);
    float HdotL = clamp(dot(H, L), 0, 1);
    float HdotV = clamp(dot(H, V), 0, 1);

    float distance = length(light.xyz - vPosition);
    float intensity = light_intensity * max(min(1.0 - pow(distance / light_range, 4.0), 1.0), 0.0) / pow(distance, 2.0);

    vec3 material_colour = vec3(0, 0, 0);
    if (NdotL > 0.0 || NdotV > 0.0)
    {
        float visibility = ( heaviside( HdotL ) / ( abs( NdotL ) + sqrt( alpha_squared + ( 1.0 - alpha_squared ) * ( NdotL * NdotL ) ) ) ) * ( heaviside( HdotV ) / ( abs( NdotV ) + sqrt( alpha_squared + ( 1.0 - alpha_squared ) * ( NdotV * NdotV ) ) ) );

        float specular_brdf = intensity * NdotL * visibility * distribution;

        vec3 diffuse_brdf = intensity * NdotL * (1 / PI) * base_colour.rgb;

        // NOTE(marco): f0 in the formula notation refers to the base colour here
        vec3 conductor_fresnel = specular_brdf * ( base_colour.rgb + ( 1.0 - base_colour.rgb ) * pow( 1.0 - abs( HdotV ), 5 ) );

        // NOTE(marco): f0 in the formula notation refers to the value derived from ior = 1.5
        float f0 = 0.04; // pow( ( 1 - ior ) / ( 1 + ior ), 2 )
        float fr = f0 + ( 1 - f0 ) * pow(1 - abs( HdotV ), 5 );
        vec3 fresnel_mix = mix( diffuse_brdf, vec3( specular_brdf ), fr );

        material_colour = mix( fresnel_mix, conductor_fresnel, metalness );
    }

    frag_color = vec4( encode_srgb( material_colour ), base_colour.a );
}
```

The steps are similar for all shaders making use of bindless resources, and it would be beneficial to have them defined in a common header. Unfortunately, this is not fully supported by the OpenGL Shading Language, or GLSL.

The first thing to do is to enable the nonuniform qualifier in the GLSL code:

```
#extension GL_EXT_nonuniform_qualifier : enable
```

This will enable the extension in the current shader, not globally; thus, it must be written in every shader.

The following code is the declaration of the proper bindless textures, with a catch:

```
layout ( set = 1, binding = 10 ) uniform sampler2D global_
textures[];
layout ( set = 1, binding = 10 ) uniform sampler3D global_
textures_3d[];
```

This is a known trick to alias the texture declarations to the same binding point. This allows us to have just one global bindless texture array, but all kinds of textures (one-dimensional, two-dimensional, three-dimensional, and their array counterparts) are supported in one go! This simplifies the usage of bindless textures across the engine and the shaders.

Finally, to read the texture, the code in the shader has to be modified as follows:

```
texture(global_textures[nonuniformEXT(texture_index)], vTexcoord0)
```

Let’s go in the following order:

1. First of all, we need the integer index coming from a constant. In this case, ``texture_index`` will contain the same number as the texture position in the bindless array.

2. Second, and this is the crucial change, we need to wrap the index with the ``nonuniformEXT`` qualifier (https://github.com/KhronosGroup/GLSL/blob/master/extensions/ext/GL_EXT_nonuniform_qualifier.txt); this will basically synchronize the programs between the different executions to properly read the texture index, in case the index is different across different threads of the same shader invocation. This might sound complicated at first but think about it as a multithreading issue that needs synchronization to make sure the proper texture index is read in each thread and, as a result, the correct texture is used.

3. Lastly, using the synchronized index we read from the global_textures array, we finally have the texture sample we wanted! 

### Conclusion

We have now added bindless textures support to the Raptor Engine! We started by checking whether the GPU supports this feature. Then we detailed the changes we made to the creation of the descriptor pool and descriptor set. Finally, we have shown how the descriptor set is updated as new textures are uploaded to the GPU and the necessary shader modifications to make use of bindless textures. All the rendering from now on will use this feature; thus, this concept will become familiar.

Next, we are going to improve our engine capabilities by adding automatic pipeline generation by  parsing shaders’ binary data.

## Automating pipeline layout generation

In this section, we are going to take advantage of the data provided by the SPIR-V binary format to extract the information needed to create a pipeline layout. SPIR-V is the intermediate representation (IR) that shader sources are compiled to before being passed to the GPU. Compared to standard GLSL shader sources, which are plain text, SPIR-V is a binary format. This means it’s a more compact format to use when distributing an application. More importantly, developers don’t have to worry about their shaders getting compiled into a different set of high-level instructions depending on the GPU and driver their code is running on.

However, a SPIR-V binary does not contain the final instructions that will be executed by the GPU. Every GPU will take a SPIR-V blob and do a final compilation into GPU instructions. This step is still required because different GPUs and driver versions can produce different assemblies for the same SPIR-V binary. Having SPIR-V as an intermediate step is still a great improvement. Shader code validation and parsing are done offline, and developers can compile their shaders together with their application code. This allows us to spot any syntax mistakes before trying to run the shader code.

Another benefit of having an intermediate representation is being able to compile shaders written in different languages to SPIR-V so that they can be used with Vulkan. It’s possible, for instance, to compile a shader written in HLSL into SPIR-V and reuse it in a Vulkan renderer. Before this option was available, developers either had to port the code manually or had to rely on tools that rewrote the shader from one language to another.

We are now going to explain how to use the information in the binary data to automatically generate a pipeline layout. You already know, from the vulkan introduction, how to compile a shader to a spv file. It is possible to read the content of this file, using the command:

```
spirv-dis main.vert.spv
```

This command will print the disassembled SPIR-V on the Terminal. We are now going to examine the relevant sections of the output.

### Understanding the SPIR-V output

Starting from the top of the output, the following is the first set of information we are provided with:
```
OpCapability Shader
%1 = OpExtInstImport "GLSL.std.450"
OpMemoryModel Logical GLSL450
OpEntryPoint Vertex %main "main" %_ %position
%vPosition %vTexcoord0 %texCoord0 %vNormal %normal %vTangent %tangent
OpSource GLSL 450
OpName %main "main"
```

This preamble defines the version of GLSL that was used to write the shader. The ``OpEntryPoint`` directive references the main function and lists the inputs and outputs for the shader. The convention is for variables to be prefixed by %, and it’s possible to forward declare a variable that is defined later.

The next section defines the output variables that are available in this shader:
```
OpName %gl_PerVertex "gl_PerVertex"
OpMemberName %gl_PerVertex 0 "gl_Position"
OpMemberName %gl_PerVertex 1 "gl_PointSize"
OpMemberName %gl_PerVertex 2 "gl_ClipDistance"
OpMemberName %gl_PerVertex 3 "gl_CullDistance"
OpName %_ ""
```

These are variables that are automatically injected by the compiler and are defined by the GLSL specification. We can see we have a gl_PerVertex structure, which in turn has four members: `gl_Position, gl_PointSize, gl_ClipDistance, and gl_CullDistance`. There is also an unnamed variable defined as %_. We’re going to discover soon what it refers to.

We now move on to the structures we have defined:
```
OpName %LocalConstants "LocalConstants"
OpMemberName %LocalConstants 0 "model"
OpMemberName %LocalConstants 1 "view_projection"
OpMemberName %LocalConstants 2 "model_inverse"
OpMemberName %LocalConstants 3 "eye"
OpMemberName %LocalConstants 4 "light"
OpName %__0 ""
```

Here, we have the entries for our LocalConstants uniform buffer, its members, and their position within the struct. We see again an unnamed %__0 variable. We’ll get to it shortly. SPIR-V allows you to define member decorations to provide additional information that is useful to determine the data layout and location within the struct:
```
OpMemberDecorate %LocalConstants 0 ColMajor
OpMemberDecorate %LocalConstants 0 Offset 0
OpMemberDecorate %LocalConstants 0 MatrixStride 16
OpMemberDecorate %LocalConstants 1 ColMajor
OpMemberDecorate %LocalConstants 1 Offset 64
OpMemberDecorate %LocalConstants 1 MatrixStride 16
OpMemberDecorate %LocalConstants 2 ColMajor
OpMemberDecorate %LocalConstants 2 Offset 128
OpMemberDecorate %LocalConstants 2 MatrixStride 16
OpMemberDecorate %LocalConstants 3 Offset 192
OpMemberDecorate %LocalConstants 4 Offset 208
OpDecorate %LocalConstants Block
```

The next two lines define the descriptor set and binding for our struct:
```
OpDecorate %__0 DescriptorSet 0
OpDecorate %__0 Binding 0
```
As you can see, these decorations refer to the unnamed %__0 variable. We have now reached the section where the variable types are defined:
```
%float = OpTypeFloat 32
%v4float = OpTypeVector %float 4
%uint = OpTypeInt 32 0
%uint_1 = OpConstant %uint 1
%_arr_float_uint_1 = OpTypeArray %float %uint_1
%gl_PerVertex = OpTypeStruct %v4float %float %_arr_float_uint_1 %_arr_float_uint_1
%_ptr_Output_gl_PerVertex = OpTypePointer Output %gl_PerVertex
%_ = OpVariable %_ptr_Output_gl_PerVertex Output
```

For each variable, we have its type and, depending on the type, additional information that is relevant to it. For instance, the %float variable is of type 32-bit float; the %v4float variable is of type vector, and it contains 4 %float values. This corresponds to vec4 in GLSL. We then have a constant definition for an unsigned value of 1 and a fixed-sized array of the float type and length of 1.

The definition of the %gl_PerVertex variable follows. It is of the struct type and, as we have seen previously, it has four members. Their types are vec4 for gl_Position, float for gl_PointSize, and float[1] for gl_ClipDistance and gl_CullDistance.

The SPIR-V specs require that each variable that can be read or written to is referred to by a pointer. And that’s exactly what we see with `%_ptr_Output_gl_PerVertex`: it’s a pointer to the gl_PerVertex struct. Finally, we can see the type for the unnamed %_ variable is a pointer to the 
gl_PerVertex struct.

Finally, we have the type definitions for our own uniform data:
```
%LocalConstants = OpTypeStruct %mat4v4float %mat4v4float %mat4v4float %v4float %v4float
%_ptr_Uniform_LocalConstants = OpTypePointer Uniform %LocalConstants
%__0 = OpVariable %_ptr_Uniform_LocalConstants Uniform
```

As before, we can see that %LocalConstants is a struct with five members, three of the mat4 type and two of the vec4 type. We then have the type definition of the pointer to our uniform struct and finally, the %__0 variable of this type. Notice that this variable has the Uniform attribute. This means it is read-only and we will make use of this information later to determine the type of descriptor to add to the pipeline layout.

The rest of the disassembly contains the input and output variable definitions. Their definition follows the same structure as the variables we have seen so far, so we are not going to analyze them all here. The disassembly also contains the instructions for the body of the shader. While it is interesting to see how the GLSL code is translated into SPIR-V instructions, this detail is not relevant to the pipeline creation, and we are not going to cover it here.

Next, we are going to show how we can leverage all of this data to automate pipeline creation.

### From SPIR-V to pipeline layout

Even if Khrono provides a parser for SPIR-V data into pipeline layout ( https://github.com/KhronosGroup/SPIRV-Reflect ), we will create our own to see how it works.

`spirv_parser.hpp`
```
#pragma once

#include "foundation/array.hpp"
#include "graphics/gpu_resources.hpp"

#if defined(_MSC_VER)
#include <spirv-headers/spirv.h>
#else
#include <spirv_cross/spirv.h>
#endif
#include <vulkan/vulkan.h>

namespace raptor {

    struct StringBuffer;

namespace spirv {

  static const u32 MAX_SET_COUNT = 32;

  struct ParseResult {
    u32 set_count;
    DescriptorSetLayoutCreation sets[MAX_SET_COUNT];
  };

  void parse_binary( const u32* data, size_t data_size, 
    StringBuffer& name_buffer, ParseResult* parse_result );

} // namespace spirv
} // namespace raptor
```

`spirv_parser.cpp`
```
#include "graphics/spirv_parser.hpp"

#include "foundation/numerics.hpp"
#include "foundation/string.hpp"

#include <string.h>

namespace raptor {
namespace spirv {

static const u32        k_bindless_texture_binding = 10;

struct Member
{
  u32 id_index;
  u32 offset;

  StringView name;
};

struct Id
{
  SpvOp op;
  u32 set;
  u32 binding;

  // For integers and floats
  u8 width;
  u8 sign;

  // For arrays, vectors and matrices
  u32 type_index;
  u32 count;

  // For variables
  SpvStorageClass storage_class;

  // For constants
  u32 value;

  // For structs
  StringView name;
  Array<Member> members;
};

VkShaderStageFlags parse_execution_model( SpvExecutionModel model )
{
  switch ( model )
  {
    case ( SpvExecutionModelVertex ):
    {
      return VK_SHADER_STAGE_VERTEX_BIT;
    }
    case ( SpvExecutionModelGeometry ):
    {
      return VK_SHADER_STAGE_GEOMETRY_BIT;
    }
    case ( SpvExecutionModelFragment ):
    {
      return VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    case ( SpvExecutionModelKernel ):
    {
      return VK_SHADER_STAGE_COMPUTE_BIT;
    }
  }

  return 0;
}

void parse_binary( const u32* data, size_t data_size, 
  StringBuffer& name_buffer, ParseResult* parse_result ) 
{  
  RASSERT( ( data_size % 4 ) == 0 );
  u32 spv_word_count = safe_cast<u32>( data_size / 4 );

  u32 magic_number = data[ 0 ];
  RASSERT( magic_number == 0x07230203 );

  u32 id_bound = data[3];

  Allocator* allocator = &MemoryService::instance()->system_allocator;
  Array<Id> ids;
  ids.init(  allocator, id_bound, id_bound );

  memset( ids.data, 0, id_bound * sizeof( Id ) );

  VkShaderStageFlags stage;

  size_t word_index = 5;
  while ( word_index < spv_word_count ) {
    SpvOp op = ( SpvOp )( data[ word_index ] & 0xFF );
    u16 word_count = ( u16 )( data[ word_index ] >> 16 );

    switch( op ) {

      case ( SpvOpEntryPoint ):
      {
        RASSERT( word_count >= 4 );

        SpvExecutionModel model = ( SpvExecutionModel )data[ word_index + 1 ];

        stage = parse_execution_model( model );
        RASSERT( stage != 0 );

        break;
      }

      case ( SpvOpDecorate ):
      {
        RASSERT( word_count >= 3 );

        u32 id_index = data[ word_index + 1 ];
        RASSERT( id_index < id_bound );

        Id& id= ids[ id_index ];

        SpvDecoration decoration = ( SpvDecoration )data[ word_index + 2 ];
        switch ( decoration )
        {
            case ( SpvDecorationBinding ):
            {
              id.binding = data[ word_index + 3 ];
              break;
            }

            case ( SpvDecorationDescriptorSet ):
            {
              id.set = data[ word_index + 3 ];
              break;
            }
        }

        break;
      }

      case ( SpvOpMemberDecorate ):
      {
        RASSERT( word_count >= 4 );

        u32 id_index = data[ word_index + 1 ];
        RASSERT( id_index < id_bound );

        Id& id= ids[ id_index ];

        u32 member_index = data[ word_index + 2 ];

        if ( id.members.capacity == 0 ) {
          id.members.init( allocator, 64, 64 );
        }

        Member& member = id.members[ member_index ];

        SpvDecoration decoration = ( SpvDecoration )data[ word_index + 3 ];
        switch ( decoration )
        {
          case ( SpvDecorationOffset ):
          {
            member.offset = data[ word_index + 4 ];
            break;
          }
        }

        break;
      }

      case ( SpvOpName ):
      {
        RASSERT( word_count >= 3 );

        u32 id_index = data[ word_index + 1 ];
        RASSERT( id_index < id_bound );

        Id& id= ids[ id_index ];

        char* name = ( char* )( data + ( word_index + 2 ) );
        char* name_view = name_buffer.append_use( name );

        id.name.text = name_view;
        id.name.length = strlen( name_view );

        break;
      }

      case ( SpvOpMemberName ):
      {
        RASSERT( word_count >= 4 );

        u32 id_index = data[ word_index + 1 ];
        RASSERT( id_index < id_bound );

        Id& id= ids[ id_index ];

        u32 member_index = data[ word_index + 2 ];

        if ( id.members.capacity == 0 ) {
          id.members.init( allocator, 64, 64 );
        }

        Member& member = id.members[ member_index ];

        char* name = ( char* )( data + ( word_index + 3 ) );
        char* name_view = name_buffer.append_use( name );

        member.name.text = name_view;
        member.name.length = strlen( name_view );

        break;
      }

      case ( SpvOpTypeInt ):
      {
        RASSERT( word_count == 4 );

        u32 id_index = data[ word_index + 1 ];
        RASSERT( id_index < id_bound );

        Id& id= ids[ id_index ];
        id.op = op;
        id.width = ( u8 )data[ word_index + 2 ];
        id.sign = ( u8 )data[ word_index + 3 ];

        break;
      }

      case ( SpvOpTypeFloat ):
      {
        RASSERT( word_count == 3 );

        u32 id_index = data[ word_index + 1 ];
        RASSERT( id_index < id_bound );

        Id& id= ids[ id_index ];
        id.op = op;
        id.width = ( u8 )data[ word_index + 2 ];

        break;
      }

      case ( SpvOpTypeVector ):
      {
        RASSERT( word_count == 4 );

        u32 id_index = data[ word_index + 1 ];
        RASSERT( id_index < id_bound );

        Id& id= ids[ id_index ];
        id.op = op;
        id.type_index = data[ word_index + 2 ];
        id.count = data[ word_index + 3 ];

        break;
      }

      case ( SpvOpTypeMatrix ):
      {
        RASSERT( word_count == 4 );

        u32 id_index = data[ word_index + 1 ];
        RASSERT( id_index < id_bound );

        Id& id= ids[ id_index ];
        id.op = op;
        id.type_index = data[ word_index + 2 ];
        id.count = data[ word_index + 3 ];

        break;
      }

      case ( SpvOpTypeImage ):
      {
        // NOTE(marco): not sure we need this information just yet
        RASSERT( word_count >= 9 );

        break;
      }

      case ( SpvOpTypeSampler ):
      {
        RASSERT( word_count == 2 );

        u32 id_index = data[ word_index + 1 ];
        RASSERT( id_index < id_bound );

        Id& id= ids[ id_index ];
        id.op = op;

        break;
      }

      case ( SpvOpTypeSampledImage ):
      {
        RASSERT( word_count == 3 );

        u32 id_index = data[ word_index + 1 ];
        RASSERT( id_index < id_bound );

        Id& id= ids[ id_index ];
        id.op = op;

        break;
      }

      case ( SpvOpTypeArray ):
      {
        RASSERT( word_count == 4 );

        u32 id_index = data[ word_index + 1 ];
        RASSERT( id_index < id_bound );

        Id& id= ids[ id_index ];
        id.op = op;
        id.type_index = data[ word_index + 2 ];
        id.count = data[ word_index + 3 ];

        break;
      }

      case ( SpvOpTypeRuntimeArray ):
      {
        RASSERT( word_count == 3 );

        u32 id_index = data[ word_index + 1 ];
        RASSERT( id_index < id_bound );

        Id& id= ids[ id_index ];
        id.op = op;
        id.type_index = data[ word_index + 2 ];

        break;
      }

      case ( SpvOpTypeStruct ):
      {
        RASSERT( word_count >= 2 );

        u32 id_index = data[ word_index + 1 ];
        RASSERT( id_index < id_bound );

        Id& id= ids[ id_index ];
        id.op = op;

        if ( word_count > 2 ) {
          for ( u16 member_index = 0; member_index < word_count - 2; ++member_index ) {
            id.members[ member_index ].id_index = data[ word_index + member_index + 2 ];
          }
        }

        break;
      }

      case ( SpvOpTypePointer ):
      {
        RASSERT( word_count == 4 );

        u32 id_index = data[ word_index + 1 ];
        RASSERT( id_index < id_bound );

        Id& id= ids[ id_index ];
        id.op = op;
        id.type_index = data[ word_index + 3 ];

        break;
      }

      case ( SpvOpConstant ):
      {
        RASSERT( word_count >= 4 );

        u32 id_index = data[ word_index + 1 ];
        RASSERT( id_index < id_bound );

        Id& id= ids[ id_index ];
        id.op = op;
        id.type_index = data[ word_index + 2 ];
        id.value = data[ word_index + 3 ]; // NOTE(marco): we assume all constants to have maximum 32bit width

        break;
      }

      case ( SpvOpVariable ):
      {
        RASSERT( word_count >= 4 );

        u32 id_index = data[ word_index + 2 ];
        RASSERT( id_index < id_bound );

        Id& id= ids[ id_index ];
        id.op = op;
        id.type_index = data[ word_index + 1 ];
        id.storage_class = ( SpvStorageClass )data[ word_index + 3 ];

        break;
      }
    }
  }

  word_index += word_count;

  for ( u32 id_index = 0; id_index < ids.size; ++id_index ) {
    Id& id= ids[ id_index ];

    if ( id.op == SpvOpVariable ) {
      switch ( id.storage_class ) {
        case ( SpvStorageClassUniform ):
        case ( SpvStorageClassUniformConstant ):
        {
          if ( id.set == 1 && 
            ( id.binding == k_bindless_texture_binding 
            || id.binding == ( k_bindless_texture_binding + 1 )) ) {
            // These are managed by the GPU device
            continue;
          }

          // NOTE(marco): get actual type
          Id& uniform_type = ids[ ids[ id.type_index ].type_index ];

          DescriptorSetLayoutCreation& setLayout = parse_result->sets[ id.set ];
          setLayout.set_set_index( id.set );

          DescriptorSetLayoutCreation::Binding binding{ };
          binding.start = id.binding;
          binding.count = 1;

          switch ( uniform_type.op ) {
            case (SpvOpTypeStruct):
            {
              binding.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
              binding.name = uniform_type.name.text;
              break;
            }

            case (SpvOpTypeSampledImage):
            {
              binding.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
              binding.name = id.name.text;
              break;
            }
          }

          setLayout.add_binding_at_index( binding, id.binding );

          parse_result->set_count = max( parse_result->set_count, ( id.set + 1 ) );

          break;
        }
      }
    }

    id.members.shutdown();
  }

  ids.shutdown();
}

} // namespace spirv
} // namespace raptor
```

We have to define an empty spirv::ParseResult structure that will contain the result of the parsing. Its definition is quite simple:
```
struct ParseResult {
  u32 set_count;
  DescriptorSetLayoutCreation sets[MAX_SET_COUNT];
};
```

We need to predeclare that Struct into `gpu_resources.hpp`:

```
...
namespace raptor {

namespace spirv {
    struct ParseResult;
} // namespace spirv
...
```


It contains the number of sets that we identified from the binary data and the list of entries for each set. The first step of the parsing is to make sure that we are reading valid SPIR-V data:
```
u32 spv_word_count = safe_cast<u32>( data_size / 4 );
u32 magic_number = data[ 0 ];
u32 id_bound = data[3];
```

We first compute the number of 32-bit words that are included in the binary. Then we verify that the first four bytes match the magic number that identifies a SPIR-V binary. Finally, we retrieve the number of IDs that are defined in the binary.

Next, we loop over all the words in the binary to retrieve the information we need. Each ID definition starts with the Op type and the number of words that it is composed of:
```
SpvOp op = ( SpvOp )( data[ word_index ] & 0xFF );
u16 word_count = ( u16 )( data[ word_index ] >> 16 );
```

The Op type is stored in the bottom 16 bits of the word, and the word count is in the top 16 bits. Next, we parse the data for the Op types we are interested in. We start with the type of shader we are currently parsing:
```
case ( SpvOpEntryPoint ):
{
  SpvExecutionModel model = ( SpvExecutionModel )data[word_index + 1 ];
  stage = parse_execution_model( model );
  break;
}
```

We extract the execution model, translate it into a VkShaderStageFlags value, and store it in the stage variable. Next, we parse the descriptor set index and binding:
```
case ( SpvOpDecorate ):
{
  u32 id_index = data[ word_index + 1 ];
  Id& id = ids[ id_index ];
  SpvDecoration decoration = ( SpvDecoration )data[ word_index + 2 ];
  switch ( decoration )
  {
    case ( SpvDecorationBinding ):
    {
      id.binding = data[ word_index + 3 ];
      break;
    }
    case ( SpvDecorationDescriptorSet ):
    {
      id.set = data[ word_index + 3 ];
      break;
    }
  }
  break;
}
```

First, we retrieve the index of the ID. As we mentioned previously, variables can be forward declared, and we might have to update the values for the same ID multiple times. Next, we retrieve the value of the decoration. We are only interested in the descriptor set index (SpvDecorationDescriptorSet) and binding (SpvDecorationBinding) and we store their values in the entry for this ID.

We follow with an example of a variable type:
```
case ( SpvOpTypeVector ):
{
  u32 id_index = data[ word_index + 1 ];
  Id& id= ids[ id_index ];
  id.op = op;
  id.type_index = data[ word_index + 2 ];
  id.count = data[ word_index + 3 ];
  break;
}
```

As we saw in the disassembly, a vector is defined by its entry type and count. We store them in the type_index and count members of the ID struct. Here, we also see how an ID can refer to another one if needed. The type_index member stores the index to another entry in the ids array and can be used later to retrieve additional type information.

Next, we have a sampler definition:
```
case ( SpvOpTypeSampler ):
{
  u32 id_index = data[ word_index + 1 ];
  RASSERT( id_index < id_bound );
  Id& id= ids[ id_index ];
  id.op = op;
  break;
}
```
We only need to store the Op type for this entry. 

Finally, we have the entry for a variable type:
```
case ( SpvOpVariable ):
{
  u32 id_index = data[ word_index + 2 ];
  Id& id= ids[ id_index ];
  id.op = op;
  id.type_index = data[ word_index + 1 ];
  id.storage_class = ( SpvStorageClass )data[
  word_index + 3 ];
  break;
}
```
The relevant information for this entry is type_index, which will always refer to an entry of pointer type and the storage class. The storage class tells us which entries are variables that we are interested in and which ones we can skip.

And that is exactly what the next part of the code is doing. Once we finish parsing all IDs, we loop over each ID entry and identify the ones we are interested in. We first identify all variables:
```
for ( u32 id_index = 0; id_index < ids.size; ++id_index ) {
 Id& id= ids[ id_index ];
 if ( id.op == SpvOpVariable ) {
```
Next, we use the variable storage class to determine whether it is a uniform variable:
```
switch ( id.storage_class ) {
 case ( SpvStorageClassUniform ):
 case ( SpvStorageClassUniformConstant ):
 {
```
We are only interested in the Uniform and UniformConstant variables. We then retrieve the 
uniform type. Remember, there is a double indirection to retrieve the actual type of a variable: first, 
we get the pointer type, and from the pointer type, we get to the real type of the variable. We 
have highlighted the code that does this:

```
Id& uniform_type = ids[ ids[ id.type_index ].type_index ];

DescriptorSetLayoutCreation& setLayout = cparse_result->sets[ id.set ];
setLayout.set_set_index( id.set );
DescriptorSetLayoutCreation::Binding binding{ };

binding.start = id.binding;
binding.count = 1;
```

After retrieving the type, we get the DescriptorSetLayoutCreation entry for the set this variable is part of. We then create a new binding entry and store the binding value. We always assume a count of 1 for each resource.

In this last step, we determine the resource type for this binding and add its entry to the set layout:
```
switch ( uniform_type.op ) {
  case (SpvOpTypeStruct):
  {
    binding.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    binding.name = uniform_type.name.text;
    break;
  }
  case (SpvOpTypeSampledImage):
  {
    binding.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.name = id.name.text;
    break;
  }
}
setLayout.add_binding_at_index( binding, id.binding );
```

We use the Op type to determine the type of resource we have found. So far, we are only interested in Struct for uniform buffers and SampledImage for textures. We are going to add support for more types in further lessons.

While it’s possible to distinguish between uniform buffers and storage buffers, the binary data cannot determine whether a buffer is dynamic or not. In our implementation, the application code needs to specify this detail. An alternative would be to use a naming convention (prefixing dynamic buffers with dyn_, for  instance) so that dynamic buffers can be automatically identified.

Note that the last line of code won't compile you need to create the `add_binding_at_index` function into your code:

`gpu_resources.hpp`
```
...
struct DescriptorSetLayoutCreation {
...
  // Building helpers
...
  DescriptorSetLayoutCreation& add_binding_at_index( const Binding& binding, int index );
...
}; // struct DescriptorSetLayoutCreation
```

`gpu_resources.cpp`
```
DescriptorSetLayoutCreation& DescriptorSetLayoutCreation::add_binding_at_index( 
  const Binding& binding, int index 
) {
  bindings[index] = binding;
  num_bindings = (index + 1) > num_bindings ? (index + 1) : num_bindings;
  return *this;
}
```
We will now update the device:

`gpu_device.cpp`
```
#include "graphics/spirv_parser.hpp"
...
void CommandBufferRing::init( GpuDevice* gpu_ ) {
  ...
  for ( u32 i = 0; i < k_max_buffers; i++ ) {
    ...
    command_buffers[ i ].gpu_device = gpu;
    command_buffers[ i ].init( QueueType::Enum::Graphics, 0, 0, false );
    command_buffers[ i ].handle = i;
  }
}
...
void CommandBufferRing::shutdown() {
    for ( u32 i = 0; i < k_max_swapchain_images * k_max_threads; i++ ) {
      vkDestroyCommandPool( gpu->vulkan_device, vulkan_command_pools[ i ], gpu->vulkan_allocation_callbacks );
    }
    for ( u32 i = 0; i < k_max_buffers; i++ ) {
      command_buffers[ i ].terminate();
    }
}
...
ShaderStateHandle GpuDevice::create_shader_state( const ShaderStateCreation& creation ) {
  ...
  sizet current_temporary_marker = temporary_allocator->get_marker();

  StringBuffer name_buffer;
  name_buffer.init( 4096, temporary_allocator );

  // Parse result needs to be always in memory as its used to free descriptor sets.
  shader_state->parse_result = ( spirv::ParseResult* )allocator->allocate( sizeof( spirv::ParseResult ), 64 );
  memset( shader_state->parse_result, 0, sizeof( spirv::ParseResult ) );
  
  for ( compiled_shaders = 0; compiled_shaders < creation.stages_count; ++compiled_shaders ) {
        ...
        spirv::parse_binary( shader_create_info.pCode, shader_create_info.codeSize, 
          name_buffer, shader_state->parse_result );
        
        set_resource_name( VK_OBJECT_TYPE_SHADER_MODULE, 
          ( u64 )shader_state->shader_stage_info[ compiled_shaders ].module, creation.name );
    }
  ...
}

void GpuDevice::destroy_pipeline( PipelineHandle pipeline ) {
  ...
  Pipeline* v_pipeline = access_pipeline( pipeline );

  ShaderState* shader_state_data = access_shader_state( v_pipeline->shader_state );
  for ( u32 l = 0; l < shader_state_data->parse_result->set_count; ++l ) {
      destroy_descriptor_set_layout( v_pipeline->descriptor_set_layout_handle[ l ] );
  }
  ...
}
...
void GpuDevice::destroy_shader_state( ShaderStateHandle shader ) {
  if ( shader.index < shaders.pool_size ) {
    resource_deletion_queue.push( { ResourceDeletionType::ShaderState, shader.index, current_frame } );

    ShaderState* state = access_shader_state( shader );

    allocator->deallocate( state->parse_result );
  } else {
    ...
}
```

We also need to update the pipeline layout creation. By the way, we change the name of a variable and add a new one:

`gpu_resourses.hpp`
```
struct Pipeline {
...
    const DesciptorSetLayout* descriptor_set[ k_max_descriptor_set_layouts ];
...
}; // struct PipelineVulkan

struct ShaderState {
...
    spirv::ParseResult* parse_result;
}; // struct ShaderStateVulkan
```

`gpu_device.cpp`
```
u32 num_active_layouts = shader_state_data->parse_result->set_count;

// Create VkPipelineLayout
for ( u32 l = 0; l < shader_state_data->parse_result->set_count; ++l ) {
  pipeline->descriptor_set_layout_handle[ l ] = create_descriptor_set_layout( shader_state_data->parse_result->sets[ l ] );
  pipeline->descriptor_set[ l ] = access_descriptor_set_layout( pipeline->descriptor_set_layout_handle[ l ] );

  vk_layouts[ l ] = pipeline->descriptor_set[ l ]->vk_descriptor_set_layout;
}

// Add bindless resource layout after other layouts.
// [TAG: BINDLESS]
```

We will also change two lines of code for ImGUI:
`raptor_imgui.cpp`
```
if ( gpu->bindless_supported ) {
  descriptor_set_layout_creation
    .add_binding( { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, 1, "LocalConstants" } )
    .add_binding( { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10, 1, "Texture" } )
    .set_name( "RLL_ImGui" );
}
else {
  descriptor_set_layout_creation
    .add_binding( { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, 1, "LocalConstants" } )
    .set_name( "RLL_ImGui" );
  // LINE DELETED
}
...
// Create descriptor set
DescriptorSetCreation ds_creation{};
if ( gpu->bindless_supported ) {       //  INVERSE CONDITION
    ds_creation.set_layout( pipeline_creation.descriptor_set_layout[0] )
    .buffer( g_ui_cb, 0 )
    .texture( g_font_texture, 1 )
    .set_name( "RL_ImGui" );
} else {
    ds_creation.set_layout( pipeline_creation.descriptor_set_layout[0] )
    .buffer( g_ui_cb, 0 )
    .set_name( "RL_ImGui" );
}
```

This concludes our introduction to the SPIR-V binary format. It might take a couple of readings to fully understand how it works, but don’t worry, it certainly took us a few iterations to fully understand it!

Knowing how to parse SPIR-V data is an important tool to automate other aspects of graphics development. It can be used, for instance, to automate the generation of C++ headers to keep CPU and GPU structs in sync. You could expand this implementation to add support for the features you might need! 

We will now take time to add mipmap support into our code, before finishing the lesson with pipeline caching.

## Adding mipmap support

The steps are similar to what we did in the vulkan introduction lesson.

`gpu_device.cpp`
```
static void vulkan_create_texture( GpuDevice& gpu, const TextureCreation& creation, TextureHandle handle, Texture* texture ) {
...
  } else {
    image_info.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    image_info.usage |= is_render_target ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT : 0;
  }
  ...
  info.subresourceRange.levelCount = creation.mipmaps;
  ...
}
...
TextureHandle GpuDevice::create_texture( const TextureCreation& creation ) {
...
  region.imageExtent = { creation.width, creation.height, creation.depth };

  // Copy from the staging buffer to the image
  add_image_barrier( command_buffer->vk_command_buffer, texture->vk_image, 
    RESOURCE_STATE_UNDEFINED, RESOURCE_STATE_COPY_DEST, 0, 1, false );

  vkCmdCopyBufferToImage( command_buffer->vk_command_buffer, staging_buffer, 
    texture->vk_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );
  // Prepare first mip to create lower mipmaps
  if ( creation.mipmaps > 1 ) {
    add_image_barrier( command_buffer->vk_command_buffer, texture->vk_image, 
      RESOURCE_STATE_COPY_DEST, RESOURCE_STATE_COPY_SOURCE, 0, 1, false );
  }

  i32 w = creation.width;
  i32 h = creation.height;

  for ( int mip_index = 1; mip_index < creation.mipmaps; ++mip_index ) {
      add_image_barrier( command_buffer->vk_command_buffer, texture->vk_image, 
        RESOURCE_STATE_UNDEFINED, RESOURCE_STATE_COPY_DEST, mip_index, 1, false );

      VkImageBlit blit_region{ };
      blit_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      blit_region.srcSubresource.mipLevel = mip_index - 1;
      blit_region.srcSubresource.baseArrayLayer = 0;
      blit_region.srcSubresource.layerCount = 1;

      blit_region.srcOffsets[0] = { 0, 0, 0 };
      blit_region.srcOffsets[1] = { w, h, 1 };

      w /= 2;
      h /= 2;

      blit_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      blit_region.dstSubresource.mipLevel = mip_index;
      blit_region.dstSubresource.baseArrayLayer = 0;
      blit_region.dstSubresource.layerCount = 1;

      blit_region.dstOffsets[0] = { 0, 0, 0 };
      blit_region.dstOffsets[1] = { w, h, 1 };

      vkCmdBlitImage( command_buffer->vk_command_buffer, texture->vk_image,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, texture->vk_image, 
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit_region, VK_FILTER_LINEAR );

      // Prepare current mip for next level
      add_image_barrier( command_buffer->vk_command_buffer, texture->vk_image,
      RESOURCE_STATE_COPY_DEST, RESOURCE_STATE_COPY_SOURCE, mip_index, 1, false );
  }

  // Transition
  add_image_barrier( command_buffer->vk_command_buffer, texture->vk_image, 
    (creation.mipmaps > 1) ? RESOURCE_STATE_COPY_SOURCE : RESOURCE_STATE_COPY_DEST, 
    RESOURCE_STATE_SHADER_RESOURCE, 0, creation.mipmaps, false );

  vkEndCommandBuffer( command_buffer->vk_command_buffer );
...
```

In the next and final section of this lesson, we are going to add pipeline caching to our GPU device implementation.

## Improving load times with a pipeline cache
























u32 queue_family_count = 0;
vkGetPhysicalDeviceQueueFamilyProperties(
  vulkan_physical_device, &queue_family_count, nullptr
);

VkQueueFamilyProperties* queue_families =
  ( VkQueueFamilyProperties* )ralloca(
    sizeof( VkQueueFamilyProperties ) * queue_family_count, allocator
  );
vkGetPhysicalDeviceQueueFamilyProperties(
  vulkan_physical_device, &queue_family_count, queue_families );

u32 family_index = 0;
for ( ; family_index < queue_family_count; ++family_index ) {
    VkQueueFamilyProperties queue_family = queue_families[ family_index ];
    if ( queue_family.queueCount > 0 && queue_family.queueFlags & ( VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT ) ) {
        //indices.graphicsFamily = i;
        break;
    }

    //VkBool32 presentSupport = false;
    //vkGetPhysicalDeviceSurfaceSupportKHR( vulkan_physical_device, i, _surface, &presentSupport );
    //if ( queue_family.queueCount && presentSupport ) {
    //    indices.presentFamily = i;
    //}

    //if ( indices.isComplete() ) {
    //    break;
    //}
}

rfree( queue_families, allocator );
