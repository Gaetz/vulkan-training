#include "Renderer.hpp"


#include "Renderer.hpp"

#include "CommandBuffer.hpp"

#include "Memory.hpp"
#include "File.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "../external/stb_image.h"


// Resource Loaders ///////////////////////////////////////////////////////

struct TextureLoader : public ResourceLoader
{

    Resource* Get(cstring name) override;
    Resource* Get(u64 hashed_name) override;

    Resource* Unload(cstring name) override;

    Resource* CreateFromFile(cstring name, cstring filename, ResourceManager* resource_manager) override;

    Renderer* renderer;
}; // struct TextureLoader

struct BufferLoader : public ResourceLoader
{

    Resource* Get(cstring name) override;
    Resource* Get(u64 hashed_name) override;

    Resource* Unload(cstring name) override;

    Renderer* renderer;
}; // struct BufferLoader

struct SamplerLoader : public ResourceLoader
{

    Resource* Get(cstring name) override;
    Resource* Get(u64 hashed_name) override;

    Resource* Unload(cstring name) override;

    Renderer* renderer;
}; // struct SamplerLoader

//
//
static TextureHandle create_texture_from_file(GpuDevice& gpu, cstring filename, cstring name)
{

    if (filename)
    {
        int comp, width, height;
        uint8_t* image_data = stbi_load(filename, &width, &height, &comp, 4);
        if (!image_data)
        {
            GPrint("Error loading texture %s", filename);
            return k_invalid_texture;
        }

        TextureCreation creation;
        creation.set_data(image_data).set_format_type(VK_FORMAT_R8G8B8A8_UNORM, TextureType::Texture2D).set_flags(1, 0).set_size((u16)width, (u16)height, 1).set_name(name);

        TextureHandle new_texture = gpu.create_texture(creation);

        // IMPORTANT:
        // Free memory loaded from file, it should not matter!
        free(image_data);

        return new_texture;
    }

    return k_invalid_texture;
}

// Renderer /////////////////////////////////////////////////////////////////////

u64 TextureResource::k_type_hash = 0;
u64 BufferResource::k_type_hash = 0;
u64 SamplerResource::k_type_hash = 0;

static TextureLoader s_texture_loader;
static BufferLoader s_buffer_loader;
static SamplerLoader s_sampler_loader;

static Renderer s_renderer;

Renderer* Renderer::Instance()
{
    return &s_renderer;
}

void Renderer::init(const RendererCreation& creation)
{

    GPrint("Renderer init\n");

    gpu = creation.gpu;

    width = gpu->swapchain_width;
    height = gpu->swapchain_height;

    textures.Init(creation.allocator, 512);
    buffers.Init(creation.allocator, 4096);
    samplers.Init(creation.allocator, 128);

    resource_cache.init(creation.allocator);

    // Init resource hashes
    TextureResource::k_type_hash = HashCalculate(TextureResource::k_type);
    BufferResource::k_type_hash = HashCalculate(BufferResource::k_type);
    SamplerResource::k_type_hash = HashCalculate(SamplerResource::k_type);

    s_texture_loader.renderer = this;
    s_buffer_loader.renderer = this;
    s_sampler_loader.renderer = this;
}

void Renderer::shutdown()
{

    resource_cache.shutdown(this);

    textures.Shutdown();
    buffers.Shutdown();
    samplers.Shutdown();

    GPrint("Renderer shutdown\n");

    gpu->shutdown();
}

void Renderer::set_loaders(ResourceManager* manager)
{

    manager->SetLoader(TextureResource::k_type, &s_texture_loader);
    manager->SetLoader(BufferResource::k_type, &s_buffer_loader);
    manager->SetLoader(SamplerResource::k_type, &s_sampler_loader);
}

void Renderer::begin_frame()
{
    gpu->new_frame();
}

void Renderer::end_frame()
{
    // Present
    gpu->present();
}

void Renderer::resize_swapchain(u32 width_, u32 height_)
{
    gpu->resize((u16)width_, (u16)height_);

    width = gpu->swapchain_width;
    height = gpu->swapchain_height;
}

f32 Renderer::aspect_ratio() const
{
    return gpu->swapchain_width * 1.f / gpu->swapchain_height;
}

BufferResource* Renderer::create_buffer(const BufferCreation& creation)
{

    BufferResource* buffer = buffers.Obtain();
    if (buffer)
    {
        BufferHandle handle = gpu->create_buffer(creation);
        buffer->handle = handle;
        buffer->name = creation.name;
        gpu->query_buffer(handle, buffer->desc);

        if (creation.name != nullptr)
        {
            resource_cache.buffers.Insert(HashCalculate(creation.name), buffer);
        }

        buffer->references = 1;

        return buffer;
    }
    return nullptr;
}

BufferResource* Renderer::create_buffer(VkBufferUsageFlags type, ResourceUsageType::Enum usage, u32 size, void* data, cstring name)
{
    BufferCreation creation{ type, usage, size, data, name };
    return create_buffer(creation);
}

TextureResource* Renderer::create_texture(const TextureCreation& creation)
{
    TextureResource* texture = textures.Obtain();

    if (texture)
    {
        TextureHandle handle = gpu->create_texture(creation);
        texture->handle = handle;
        texture->name = creation.name;
        gpu->query_texture(handle, texture->desc);

        if (creation.name != nullptr)
        {
            resource_cache.textures.Insert(HashCalculate(creation.name), texture);
        }

        texture->references = 1;

        return texture;
    }
    return nullptr;
}

TextureResource* Renderer::create_texture(cstring name, cstring filename)
{
    TextureResource* texture = textures.Obtain();

    if (texture)
    {
        TextureHandle handle = create_texture_from_file(*gpu, filename, name);
        texture->handle = handle;
        gpu->query_texture(handle, texture->desc);
        texture->references = 1;
        texture->name = name;

        resource_cache.textures.Insert(HashCalculate(name), texture);

        return texture;
    }
    return nullptr;
}

SamplerResource* Renderer::create_sampler(const SamplerCreation& creation)
{
    SamplerResource* sampler = samplers.Obtain();
    if (sampler)
    {
        SamplerHandle handle = gpu->create_sampler(creation);
        sampler->handle = handle;
        sampler->name = creation.name;
        gpu->query_sampler(handle, sampler->desc);

        if (creation.name != nullptr)
        {
            resource_cache.samplers.Insert(HashCalculate(creation.name), sampler);
        }

        sampler->references = 1;

        return sampler;
    }
    return nullptr;
}

void Renderer::destroy_buffer(BufferResource* buffer)
{
    if (!buffer)
    {
        return;
    }

    buffer->RemoveReference();
    if (buffer->references)
    {
        return;
    }

    resource_cache.buffers.Remove(HashCalculate(buffer->desc.name));
    gpu->destroy_buffer(buffer->handle);
    buffers.Release(buffer);
}

void Renderer::destroy_texture(TextureResource* texture)
{
    if (!texture)
    {
        return;
    }

    texture->RemoveReference();
    if (texture->references)
    {
        return;
    }

    resource_cache.textures.Remove(HashCalculate(texture->desc.name));
    gpu->destroy_texture(texture->handle);
    textures.Release(texture);
}

void Renderer::destroy_sampler(SamplerResource* sampler)
{
    if (!sampler)
    {
        return;
    }

    sampler->RemoveReference();
    if (sampler->references)
    {
        return;
    }

    resource_cache.samplers.Remove(HashCalculate(sampler->desc.name));
    gpu->destroy_sampler(sampler->handle);
    samplers.Release(sampler);
}

void* Renderer::map_buffer(BufferResource* buffer, u32 offset, u32 size)
{

    MapBufferParameters cb_map = { buffer->handle, offset, size };
    return gpu->map_buffer(cb_map);
}

void Renderer::unmap_buffer(BufferResource* buffer)
{

    if (buffer->desc.parent_handle.index == k_invalid_index)
    {
        MapBufferParameters cb_map = { buffer->handle, 0, 0 };
        gpu->unmap_buffer(cb_map);
    }
}

// Resource Loaders ///////////////////////////////////////////////////////

// Texture Loader /////////////////////////////////////////////////////////
Resource* TextureLoader::Get(cstring name)
{
    const u64 hashed_name = HashCalculate(name);
    return renderer->resource_cache.textures.Get(hashed_name);
}

Resource* TextureLoader::Get(u64 hashed_name)
{
    return renderer->resource_cache.textures.Get(hashed_name);
}

Resource* TextureLoader::Unload(cstring name)
{
    const u64 hashed_name = HashCalculate(name);
    TextureResource* texture = renderer->resource_cache.textures.Get(hashed_name);
    if (texture)
    {
        renderer->destroy_texture(texture);
    }
    return nullptr;
}

Resource* TextureLoader::CreateFromFile(cstring name, cstring filename, ResourceManager* resource_manager)
{
    return renderer->create_texture(name, filename);
}

// BufferLoader //////////////////////////////////////////////////////////
Resource* BufferLoader::Get(cstring name)
{
    const u64 hashed_name = HashCalculate(name);
    return renderer->resource_cache.buffers.Get(hashed_name);
}

Resource* BufferLoader::Get(u64 hashed_name)
{
    return renderer->resource_cache.buffers.Get(hashed_name);
}

Resource* BufferLoader::Unload(cstring name)
{
    const u64 hashed_name = HashCalculate(name);
    BufferResource* buffer = renderer->resource_cache.buffers.Get(hashed_name);
    if (buffer)
    {
        renderer->destroy_buffer(buffer);
    }

    return nullptr;
}

// SamplerLoader /////////////////////////////////////////////////////////
Resource* SamplerLoader::Get(cstring name)
{
    const u64 hashed_name = HashCalculate(name);
    return renderer->resource_cache.samplers.Get(hashed_name);
}

Resource* SamplerLoader::Get(u64 hashed_name)
{
    return renderer->resource_cache.samplers.Get(hashed_name);
}

Resource* SamplerLoader::Unload(cstring name)
{
    const u64 hashed_name = HashCalculate(name);
    SamplerResource* sampler = renderer->resource_cache.samplers.Get(hashed_name);
    if (sampler)
    {
        renderer->destroy_sampler(sampler);
    }
    return nullptr;
}

// ResourceCache
void ResourceCache::init(Allocator* allocator)
{
    // Init resources caching
    textures.Init(allocator, 16);
    buffers.Init(allocator, 16);
    samplers.Init(allocator, 16);
}

void ResourceCache::shutdown(Renderer* renderer)
{

    FlatHashMapIterator it = textures.IteratorBegin();

    while (it.isValid())
    {
        TextureResource* texture = textures.Get(it);
        renderer->destroy_texture(texture);

        textures.IteratorAdvance(it);
    }

    it = buffers.IteratorBegin();

    while (it.isValid())
    {
        BufferResource* buffer = buffers.Get(it);
        renderer->destroy_buffer(buffer);

        buffers.IteratorAdvance(it);
    }

    it = samplers.IteratorBegin();

    while (it.isValid())
    {
        SamplerResource* sampler = samplers.Get(it);
        renderer->destroy_sampler(sampler);

        samplers.IteratorAdvance(it);
    }

    textures.Shutdown();
    buffers.Shutdown();
    samplers.Shutdown();
}
