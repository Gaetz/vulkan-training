#include "ModelLoader.hpp"
#include "../../BasicServices/Log.h"

#define GLM_ENABLE_EXPERIMENTAL
using services::Log;
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include <tiny_obj_loader.h>
#include <unordered_map>

// =============================================================================
//  Internal vertex type for OBJ loading and deduplication.
//  Matches the vertex layout expected by the renderers using ModelLoader:
//  float3 pos, float3 color (white), float2 texCoord.
// =============================================================================
namespace {
struct ObjVertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;

    bool operator==(const ObjVertex& o) const {
        return pos == o.pos && color == o.color && texCoord == o.texCoord;
    }
};
} // namespace

namespace std {
template<> struct hash<ObjVertex> {
    size_t operator()(ObjVertex const& v) const {
        return ((hash<glm::vec3>()(v.pos)    ^
                (hash<glm::vec3>()(v.color)  << 1)) >> 1) ^
                (hash<glm::vec2>()(v.texCoord) << 1);
    }
};
} // namespace std

// =============================================================================
//  Constructor
// =============================================================================
ModelLoader::ModelLoader(vk::raii::Device& d, VmaAllocator a,
                         vk::raii::CommandPool& cp, vk::raii::Queue& q)
    : device{d}, allocator{a}, commandPool{cp}, queue{q}
{}

// =============================================================================
//  load — parse OBJ, deduplicate vertices, upload to GPU.
// =============================================================================
Model ModelLoader::load(const std::string& path) {
    tinyobj::attrib_t                attrib;
    std::vector<tinyobj::shape_t>    shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str())) {
        Log::Error("ModelLoader: failed to load '%s': %s%s",
                   path.c_str(), warn.c_str(), err.c_str());
        return {};
    }
    if (!warn.empty()) Log::Warn("ModelLoader: %s", warn.c_str());

    std::vector<ObjVertex>   vertices;
    std::vector<uint32_t>    indices;
    std::unordered_map<ObjVertex, uint32_t> unique;

    for (const auto& shape : shapes) {
        for (const auto& idx : shape.mesh.indices) {
            ObjVertex v{};
            v.pos = {
                attrib.vertices[3 * idx.vertex_index + 0],
                attrib.vertices[3 * idx.vertex_index + 1],
                attrib.vertices[3 * idx.vertex_index + 2],
            };
            v.texCoord = {
                attrib.texcoords[2 * idx.texcoord_index + 0],
                1.0f - attrib.texcoords[2 * idx.texcoord_index + 1],  // flip V
            };
            v.color = {1.0f, 1.0f, 1.0f};

            auto [it, inserted] = unique.emplace(v, uint32_t(vertices.size()));
            if (inserted) vertices.push_back(v);
            indices.push_back(it->second);
        }
    }

    Log::Info("ModelLoader: '%s' — %zu unique vertices, %zu indices.",
              path.c_str(), vertices.size(), indices.size());

    return Model::upload(allocator, device, commandPool, queue,
                         vertices.data(), sizeof(ObjVertex) * vertices.size(),
                         indices.data(),  sizeof(uint32_t)  * indices.size(),
                         uint32_t(indices.size()));
}
