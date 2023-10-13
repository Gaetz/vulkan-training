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

Now, when we call ``vulkan_create_texture``, the newly created resource gets added to the ``texture_to_update_bindless`` array:

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
