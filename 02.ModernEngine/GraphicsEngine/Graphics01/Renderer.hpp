#pragma once
#include "GpuDevice.hpp"
#include "GpuResources.hpp"

#include "ResourceManager.hpp"


struct Renderer;

//
//
struct BufferResource : public Resource
{

    BufferHandle handle;
    u32 poolIndex;
    BufferDescription desc;

    static constexpr cstring k_type = "raptor_buffer_type";
    static u64 k_type_hash;

}; // struct Buffer

//
//
struct TextureResource : public Resource
{

    TextureHandle handle;
    u32 poolIndex;
    TextureDescription desc;

    static constexpr cstring k_type = "raptor_texture_type";
    static u64 k_type_hash;

}; // struct Texture

//
//
struct SamplerResource : public Resource
{

    SamplerHandle handle;
    u32 poolIndex;
    SamplerDescription desc;

    static constexpr cstring k_type = "raptor_sampler_type";
    static u64 k_type_hash;

}; // struct Sampler

// ResourceCache ////////////////////////////////////////////////////////////////

//
//
struct ResourceCache
{

    void init(Allocator* allocator);
    void shutdown(Renderer* renderer);

    FlatHashMap<u64, TextureResource*> textures;
    FlatHashMap<u64, BufferResource*> buffers;
    FlatHashMap<u64, SamplerResource*> samplers;

}; // struct ResourceCache

// Renderer /////////////////////////////////////////////////////////////////////

struct RendererCreation
{

    GpuDevice* gpu;
    Allocator* allocator;

}; // struct RendererCreation

//
// Main class responsible for handling all high level resources
//
struct Renderer : public Service
{

    G_DECLARE_SERVICE(Renderer);

    void init(const RendererCreation& creation);
    void shutdown();

    void set_loaders(ResourceManager* manager);

    void begin_frame();
    void end_frame();

    void resize_swapchain(u32 width, u32 height);

    f32 aspect_ratio() const;

    // Creation/destruction
    BufferResource* create_buffer(const BufferCreation& creation);
    BufferResource* create_buffer(VkBufferUsageFlags type, ResourceUsageType::Enum usage, u32 size, void* data, cstring name);

    TextureResource* create_texture(const TextureCreation& creation);
    TextureResource* create_texture(cstring name, cstring filename);

    SamplerResource* create_sampler(const SamplerCreation& creation);

    void destroy_buffer(BufferResource* buffer);
    void destroy_texture(TextureResource* texture);
    void destroy_sampler(SamplerResource* sampler);

    // Update resources
    void* map_buffer(BufferResource* buffer, u32 offset = 0, u32 size = 0);
    void unmap_buffer(BufferResource* buffer);

    CommandBuffer* get_command_buffer(QueueType::Enum type, bool begin) { return gpu->get_command_buffer(type, begin); }
    void queue_command_buffer(CommandBuffer* commands) { gpu->queue_command_buffer(commands); }

    ResourcePoolTyped<TextureResource> textures;
    ResourcePoolTyped<BufferResource> buffers;
    ResourcePoolTyped<SamplerResource> samplers;

    ResourceCache resource_cache;

    GpuDevice* gpu;

    u16 width;
    u16 height;

    static constexpr cstring k_name = "raptor_rendering_service";

}; // struct Renderer

