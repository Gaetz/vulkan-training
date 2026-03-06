#include "GltfModelLoader.hpp"
#include "../../BasicServices/Log.h"

// tinygltf implementation — defined here to avoid conflicts with stb_image
// (main.cpp already owns STB_IMAGE_IMPLEMENTATION).
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include <tiny_gltf.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>

#include <cstring>

using services::Log;

// =============================================================================
//  Internal vertex type — same layout as the OBJ loader so both produce
//  Models that are compatible with the same renderer pipelines.
// =============================================================================
namespace {
struct GltfVertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;
};

// Read a single float component from an accessor, handling interleaved strides.
float readFloat(const tinygltf::Model& model, const tinygltf::Accessor& acc,
                size_t vertexIndex, int component) {
    const auto& bv  = model.bufferViews[acc.bufferView];
    const auto& buf = model.buffers[bv.buffer];

    const size_t numComponents =
        static_cast<size_t>(tinygltf::GetNumComponentsInType(acc.type));
    const size_t stride =
        bv.byteStride != 0
            ? bv.byteStride
            : sizeof(float) * numComponents;

    const size_t byteOffset =
        bv.byteOffset + acc.byteOffset
        + vertexIndex * stride
        + static_cast<size_t>(component) * sizeof(float);

    float val = 0.0f;
    std::memcpy(&val, buf.data.data() + byteOffset, sizeof(float));
    return val;
}
} // namespace

// =============================================================================
//  Constructor
// =============================================================================
GltfModelLoader::GltfModelLoader(vk::raii::Device& d, VmaAllocator a,
                                 vk::raii::CommandPool& cp, vk::raii::Queue& q)
    : device{d}, allocator{a}, commandPool{cp}, queue{q}
{}

// =============================================================================
//  load — parse GLTF, extract first mesh/primitive, upload to GPU.
// =============================================================================
Model GltfModelLoader::load(const std::string& path) {
    tinygltf::TinyGLTF loader;
    tinygltf::Model    model;
    std::string        warn, err;

    // Detect binary (.glb) vs text (.gltf) by extension.
    const bool isGlb = path.size() >= 4 &&
                       path.substr(path.size() - 4) == ".glb";

    const bool ok = isGlb
        ? loader.LoadBinaryFromFile(&model, &err, &warn, path)
        : loader.LoadASCIIFromFile (&model, &err, &warn, path);

    if (!warn.empty()) Log::Warn("GltfModelLoader: %s", warn.c_str());
    if (!ok) {
        Log::Error("GltfModelLoader: failed to load '%s': %s",
                   path.c_str(), err.c_str());
        return {};
    }
    if (model.meshes.empty()) {
        Log::Error("GltfModelLoader: '%s' contains no meshes.", path.c_str());
        return {};
    }

    const auto& prim = model.meshes[0].primitives[0];

    // -------------------------------------------------------------------------
    // Positions (required)
    // -------------------------------------------------------------------------
    auto posIt = prim.attributes.find("POSITION");
    if (posIt == prim.attributes.end()) {
        Log::Error("GltfModelLoader: no POSITION attribute in '%s'.", path.c_str());
        return {};
    }
    const tinygltf::Accessor& posAcc = model.accessors[posIt->second];

    // -------------------------------------------------------------------------
    // Texture coordinates (optional — default to 0,0)
    // -------------------------------------------------------------------------
    const tinygltf::Accessor* uvAcc = nullptr;
    auto uvIt = prim.attributes.find("TEXCOORD_0");
    if (uvIt != prim.attributes.end())
        uvAcc = &model.accessors[uvIt->second];

    // -------------------------------------------------------------------------
    // Build vertex array
    // -------------------------------------------------------------------------
    const size_t numVertices = posAcc.count;
    std::vector<GltfVertex> vertices;
    vertices.reserve(numVertices);

    for (size_t i = 0; i < numVertices; ++i) {
        GltfVertex v{};
        v.pos   = { readFloat(model, posAcc, i, 0),
                    readFloat(model, posAcc, i, 1),
                    readFloat(model, posAcc, i, 2) };
        v.color = { 1.0f, 1.0f, 1.0f };

        if (uvAcc) {
            v.texCoord = { readFloat(model, *uvAcc, i, 0),
                           readFloat(model, *uvAcc, i, 1) };
        }
        vertices.push_back(v);
    }

    // -------------------------------------------------------------------------
    // Build index array (supports uint8, uint16, uint32)
    // -------------------------------------------------------------------------
    std::vector<uint32_t> indices;

    if (prim.indices >= 0) {
        const auto& idxAcc = model.accessors[prim.indices];
        const auto& idxBV  = model.bufferViews[idxAcc.bufferView];
        const auto& idxBuf = model.buffers[idxBV.buffer];
        const uint8_t* raw =
            idxBuf.data.data() + idxBV.byteOffset + idxAcc.byteOffset;

        indices.reserve(idxAcc.count);
        for (size_t i = 0; i < idxAcc.count; ++i) {
            switch (idxAcc.componentType) {
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                    indices.push_back(static_cast<uint32_t>(raw[i]));
                    break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                    indices.push_back(
                        static_cast<uint32_t>(
                            reinterpret_cast<const uint16_t*>(raw)[i]));
                    break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                    indices.push_back(
                        reinterpret_cast<const uint32_t*>(raw)[i]);
                    break;
                default:
                    Log::Warn("GltfModelLoader: unknown index component type %d.",
                              idxAcc.componentType);
                    indices.push_back(static_cast<uint32_t>(i));
                    break;
            }
        }
    } else {
        // Non-indexed mesh — generate sequential indices.
        indices.resize(numVertices);
        for (size_t i = 0; i < numVertices; ++i)
            indices[i] = static_cast<uint32_t>(i);
    }

    Log::Info("GltfModelLoader: '%s' — %zu vertices, %zu indices.",
              path.c_str(), vertices.size(), indices.size());

    return Model::upload(allocator, device, commandPool, queue,
                         vertices.data(), sizeof(GltfVertex) * vertices.size(),
                         indices.data(),  sizeof(uint32_t)  * indices.size(),
                         static_cast<uint32_t>(indices.size()));
}
