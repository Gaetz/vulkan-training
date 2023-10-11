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

  Array<ResourceUpdate> texture_to_update_bindless;
```
`gpu_device.cpp`
```
static const u32 k_bindless_texture_binding = 10;
static const u32 k_max_bindless_resources = 1024;
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

  check_result( vkAllocateDescriptorSets(
    vulkan_device, &alloc_info, &vulkan_bindless_descriptor_set
  ) );
}
```

### Updateing the descriptor set

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
