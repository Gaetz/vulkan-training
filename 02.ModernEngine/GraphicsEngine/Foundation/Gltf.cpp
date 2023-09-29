#include "Gltf.hpp"

#include "../external/json.hpp"

#include "Assert.hpp"
#include "File.hpp"

using json = nlohmann::json;

static void* AllocateAndZero(Allocator* allocator, sizet size) {
    void* result = allocator->Allocate(size, 64);
    memset(result, 0, size);

    return result;
}

static void TryLoadString(json& jsonData, cstring key, StringBuffer& stringBuffer, Allocator* allocator) {
    auto it = jsonData.find(key);
    if (it == jsonData.end())
        return;

    std::string value = jsonData.value(key, "");

    stringBuffer.Init(value.length() + 1, allocator);
    stringBuffer.Append(value.c_str());
}

static void TryLoadInt(json& jsonData, cstring key, i32& value) {
    auto it = jsonData.find(key);
    if (it == jsonData.end())
    {
        value = glTF::INVALID_INT_VALUE;
        return;
    }

    value = jsonData.value(key, 0);
}

static void TryLoadFloat(json& jsonData, cstring key, f32& value) {
    auto it = jsonData.find(key);
    if (it == jsonData.end())
    {
        value = glTF::INVALID_FLOAT_VALUE;
        return;
    }

    value = jsonData.value(key, 0.0f);
}

static void TryLoadBool(json& jsonData, cstring key, bool& value) {
    auto it = jsonData.find(key);
    if (it == jsonData.end())
    {
        value = false;
        return;
    }

    value = jsonData.value(key, false);
}

static void TryLoadType(json& jsonData, cstring key, glTF::Accessor::Type& type) {
    std::string value = jsonData.value(key, "");
    if (value == "SCALAR") {
        type = glTF::Accessor::Type::Scalar;
    }
    else if (value == "VEC2") {
        type = glTF::Accessor::Type::Vec2;
    }
    else if (value == "VEC3") {
        type = glTF::Accessor::Type::Vec3;
    }
    else if (value == "VEC4") {
        type = glTF::Accessor::Type::Vec4;
    }
    else if (value == "MAT2") {
        type = glTF::Accessor::Type::Mat2;
    }
    else if (value == "MAT3") {
        type = glTF::Accessor::Type::Mat3;
    }
    else if (value == "MAT4") {
        type = glTF::Accessor::Type::Mat4;
    }
    else {
        GASSERT(false);
    }
}

static void TryLoadIntArray(json& jsonData, cstring key, u32& count, i32** array, Allocator* allocator) {
    auto it = jsonData.find(key);
    if (it == jsonData.end()) {
        count = 0;
        *array = nullptr;
        return;
    }

    json jsonArray = jsonData.at(key);

    count = jsonArray.size();

    i32* values = (i32*)AllocateAndZero(allocator, sizeof(i32) * count);

    for (sizet i = 0; i < count; ++i) {
        values[i] = jsonArray.at(i);
    }

    *array = values;
}

static void TryLoadFloatArray(json& jsonData, cstring key, u32& count, float** array, Allocator* allocator) {
    auto it = jsonData.find(key);
    if (it == jsonData.end()) {
        count = 0;
        *array = nullptr;
        return;
    }

    json jsonArray = jsonData.at(key);

    count = jsonArray.size();

    float* values = (float*)AllocateAndZero(allocator, sizeof(float) * count);

    for (sizet i = 0; i < count; ++i) {
        values[i] = jsonArray.at(i);
    }

    *array = values;
}

static void LoadAsset(json& jsonData, glTF::Asset& asset, Allocator* allocator) {
    json json_asset = jsonData["asset"];

    TryLoadString(json_asset, "copyright", asset.copyright, allocator);
    TryLoadString(json_asset, "generator", asset.generator, allocator);
    TryLoadString(json_asset, "minVersion", asset.minVersion, allocator);
    TryLoadString(json_asset, "version", asset.version, allocator);
}

static void LoadScene(json& jsonData, glTF::Scene& scene, Allocator* allocator) {
    TryLoadIntArray(jsonData, "nodes", scene.nodes_count, &scene.nodes, allocator);
}

static void load_scenes(json& jsonData, glTF::glTF& gltfData, Allocator* allocator) {
    json scenes = jsonData["scenes"];

    sizet scene_count = scenes.size();
    gltfData.scenes = (glTF::Scene*)AllocateAndZero(allocator, sizeof(glTF::Scene) * scene_count);
    gltfData.scenes_count = scene_count;

    for (sizet i = 0; i < scene_count; ++i) {
        LoadScene(scenes[i], gltfData.scenes[i], allocator);
    }
}

static void LoadBuffer(json& jsonData, glTF::Buffer& buffer, Allocator* allocator) {
    TryLoadString(jsonData, "uri", buffer.uri, allocator);
    TryLoadInt(jsonData, "byteLength", buffer.byte_length);
    TryLoadString(jsonData, "name", buffer.name, allocator);
}

static void LoadBuffers(json& jsonData, glTF::glTF& gltfData, Allocator* allocator) {
    json buffers = jsonData["buffers"];

    sizet buffer_count = buffers.size();
    gltfData.buffers = (glTF::Buffer*)AllocateAndZero(allocator, sizeof(glTF::Buffer) * buffer_count);
    gltfData.buffers_count = buffer_count;

    for (sizet i = 0; i < buffer_count; ++i) {
        LoadBuffer(buffers[i], gltfData.buffers[i], allocator);
    }
}

static void LoadBufferView(json& jsonData, glTF::BufferView& buffer_view, Allocator* allocator) {
    TryLoadInt(jsonData, "buffer", buffer_view.buffer);
    TryLoadInt(jsonData, "byteLength", buffer_view.byte_length);
    TryLoadInt(jsonData, "byteOffset", buffer_view.byte_offset);
    TryLoadInt(jsonData, "byteStride", buffer_view.byte_stride);
    TryLoadInt(jsonData, "target", buffer_view.target);
    TryLoadString(jsonData, "name", buffer_view.name, allocator);
}

static void LoadBufferViews(json& jsonData, glTF::glTF& gltfData, Allocator* allocator) {
    json buffers = jsonData["bufferViews"];

    sizet buffer_count = buffers.size();
    gltfData.buffer_views = (glTF::BufferView*)AllocateAndZero(allocator, sizeof(glTF::BufferView) * buffer_count);
    gltfData.buffer_views_count = buffer_count;

    for (sizet i = 0; i < buffer_count; ++i) {
        LoadBufferView(buffers[i], gltfData.buffer_views[i], allocator);
    }
}

static void LoadNode(json& jsonData, glTF::Node& node, Allocator* allocator) {
    TryLoadInt(jsonData, "camera", node.camera);
    TryLoadInt(jsonData, "mesh", node.mesh);
    TryLoadInt(jsonData, "skin", node.skin);
    TryLoadIntArray(jsonData, "children", node.children_count, &node.children, allocator);
    TryLoadFloatArray(jsonData, "matrix", node.matrix_count, &node.matrix, allocator);
    TryLoadFloatArray(jsonData, "rotation", node.rotation_count, &node.rotation, allocator);
    TryLoadFloatArray(jsonData, "scale", node.scale_count, &node.scale, allocator);
    TryLoadFloatArray(jsonData, "translation", node.translation_count, &node.translation, allocator);
    TryLoadFloatArray(jsonData, "weights", node.weights_count, &node.weights, allocator);
    TryLoadString(jsonData, "name", node.name, allocator);
}

static void LoadNodes(json& jsonData, glTF::glTF& gltfData, Allocator* allocator) {
    json array = jsonData["nodes"];

    sizet array_count = array.size();
    gltfData.nodes = (glTF::Node*)AllocateAndZero(allocator, sizeof(glTF::Node) * array_count);
    gltfData.nodes_count = array_count;

    for (sizet i = 0; i < array_count; ++i) {
        LoadNode(array[i], gltfData.nodes[i], allocator);
    }
}

static void LoadMeshPrimitive(json& jsonData, glTF::MeshPrimitive& mesh_primitive, Allocator* allocator) {
    TryLoadInt(jsonData, "indices", mesh_primitive.indices);
    TryLoadInt(jsonData, "material", mesh_primitive.material);
    TryLoadInt(jsonData, "mode", mesh_primitive.mode);

    json attributes = jsonData["attributes"];

    mesh_primitive.attributes = (glTF::MeshPrimitive::Attribute*)AllocateAndZero(allocator, sizeof(glTF::MeshPrimitive::Attribute) * attributes.size());
    mesh_primitive.attribute_count = attributes.size();

    u32 index = 0;
    for (auto json_attribute : attributes.items()) {
        std::string key = json_attribute.key();
        glTF::MeshPrimitive::Attribute& attribute = mesh_primitive.attributes[index];

        attribute.key.Init(key.size() + 1, allocator);
        attribute.key.Append(key.c_str());

        attribute.accessor_index = json_attribute.value();

        ++index;
    }
}

static void LoadMeshPrimitives(json& jsonData, glTF::Mesh& mesh, Allocator* allocator) {
    json array = jsonData["primitives"];

    sizet array_count = array.size();
    mesh.primitives = (glTF::MeshPrimitive*)AllocateAndZero(allocator, sizeof(glTF::MeshPrimitive) * array_count);
    mesh.primitives_count = array_count;

    for (sizet i = 0; i < array_count; ++i) {
        LoadMeshPrimitive(array[i], mesh.primitives[i], allocator);
    }
}

static void LoadMesh(json& jsonData, glTF::Mesh& mesh, Allocator* allocator) {
    LoadMeshPrimitives(jsonData, mesh, allocator);
    TryLoadFloatArray(jsonData, "weights", mesh.weights_count, &mesh.weights, allocator);
    TryLoadString(jsonData, "name", mesh.name, allocator);
}

static void LoadMeshes(json& jsonData, glTF::glTF& gltfData, Allocator* allocator) {
    json array = jsonData["meshes"];

    sizet array_count = array.size();
    gltfData.meshes = (glTF::Mesh*)AllocateAndZero(allocator, sizeof(glTF::Mesh) * array_count);
    gltfData.meshes_count = array_count;

    for (sizet i = 0; i < array_count; ++i) {
        LoadMesh(array[i], gltfData.meshes[i], allocator);
    }
}

static void LoadAccessor(json& jsonData, glTF::Accessor& accessor, Allocator* allocator) {
    TryLoadInt(jsonData, "bufferView", accessor.buffer_view);
    TryLoadInt(jsonData, "byteOffset", accessor.byte_offset);
    TryLoadInt(jsonData, "componentType", accessor.component_type);
    TryLoadInt(jsonData, "count", accessor.count);
    TryLoadInt(jsonData, "sparse", accessor.sparse);
    TryLoadFloatArray(jsonData, "max", accessor.max_count, &accessor.max, allocator);
    TryLoadFloatArray(jsonData, "min", accessor.min_count, &accessor.min, allocator);
    TryLoadBool(jsonData, "normalized", accessor.normalized);
    TryLoadType(jsonData, "type", accessor.type);
}

static void LoadAccessors(json& jsonData, glTF::glTF& gltfData, Allocator* allocator) {
    json array = jsonData["accessors"];

    sizet array_count = array.size();
    gltfData.accessors = (glTF::Accessor*)AllocateAndZero(allocator, sizeof(glTF::Accessor) * array_count);
    gltfData.accessors_count = array_count;

    for (sizet i = 0; i < array_count; ++i) {
        LoadAccessor(array[i], gltfData.accessors[i], allocator);
    }
}

static void TryLoadTextureInfo(json& jsonData, cstring key, glTF::TextureInfo** texture_info, Allocator* allocator) {
    auto it = jsonData.find(key);
    if (it == jsonData.end()) {
        *texture_info = nullptr;
        return;
    }

    glTF::TextureInfo* ti = (glTF::TextureInfo*)allocator->Allocate(sizeof(glTF::TextureInfo), 64);

    TryLoadInt(*it, "index", ti->index);
    TryLoadInt(*it, "texCoord", ti->texCoord);

    *texture_info = ti;
}

static void TryLoadMaterialNormalTextureInfo(json& jsonData, cstring key, glTF::MaterialNormalTextureInfo** texture_info, Allocator* allocator) {
    auto it = jsonData.find(key);
    if (it == jsonData.end()) {
        *texture_info = nullptr;
        return;
    }

    glTF::MaterialNormalTextureInfo* ti = (glTF::MaterialNormalTextureInfo*)allocator->Allocate(sizeof(glTF::MaterialNormalTextureInfo), 64);

    TryLoadInt(*it, "index", ti->index);
    TryLoadInt(*it, "texCoord", ti->tex_coord);
    TryLoadFloat(*it, "scale", ti->scale);

    *texture_info = ti;
}

static void TryLoadMaterialOcclusionTextureInfo(json& jsonData, cstring key, glTF::MaterialOcclusionTextureInfo** texture_info, Allocator* allocator) {
    auto it = jsonData.find(key);
    if (it == jsonData.end()) {
        *texture_info = nullptr;
        return;
    }

    glTF::MaterialOcclusionTextureInfo* ti = (glTF::MaterialOcclusionTextureInfo*)allocator->Allocate(sizeof(glTF::MaterialOcclusionTextureInfo), 64);

    TryLoadInt(*it, "index", ti->index);
    TryLoadInt(*it, "texCoord", ti->texCoord);
    TryLoadFloat(*it, "strength", ti->strength);

    *texture_info = ti;
}

static void TryLoadMaterialPBRMetallicRoughness(json& jsonData, cstring key, glTF::MaterialPBRMetallicRoughness** texture_info, Allocator* allocator) {
    auto it = jsonData.find(key);
    if (it == jsonData.end())
    {
        *texture_info = nullptr;
        return;
    }

    glTF::MaterialPBRMetallicRoughness* ti = (glTF::MaterialPBRMetallicRoughness*)allocator->Allocate(sizeof(glTF::MaterialPBRMetallicRoughness), 64);

    TryLoadFloatArray(*it, "baseColorFactor", ti->base_color_factor_count, &ti->base_color_factor, allocator);
    TryLoadTextureInfo(*it, "baseColorTexture", &ti->base_color_texture, allocator);
    TryLoadFloat(*it, "metallicFactor", ti->metallic_factor);
    TryLoadTextureInfo(*it, "metallicRoughnessTexture", &ti->metallic_roughness_texture, allocator);
    TryLoadFloat(*it, "roughnessFactor", ti->roughness_factor);

    *texture_info = ti;
}

static void LoadMaterial(json& jsonData, glTF::Material& material, Allocator* allocator) {
    TryLoadFloatArray(jsonData, "emissiveFactor", material.emissive_factor_count, &material.emissive_factor, allocator);
    TryLoadFloat(jsonData, "alphaCutoff", material.alpha_cutoff);
    TryLoadString(jsonData, "alphaMode", material.alpha_mode, allocator);
    TryLoadBool(jsonData, "doubleSided", material.double_sided);

    TryLoadTextureInfo(jsonData, "emissiveTexture", &material.emissive_texture, allocator);
    TryLoadMaterialNormalTextureInfo(jsonData, "normalTexture", &material.normal_texture, allocator);
    TryLoadMaterialOcclusionTextureInfo(jsonData, "occlusionTexture", &material.occlusion_texture, allocator);
    TryLoadMaterialPBRMetallicRoughness(jsonData, "pbrMetallicRoughness", &material.pbr_metallic_roughness, allocator);

    TryLoadString(jsonData, "name", material.name, allocator);
}

static void LoadMaterials(json& jsonData, glTF::glTF& gltfData, Allocator* allocator) {
    json array = jsonData["materials"];

    sizet array_count = array.size();
    gltfData.materials = (glTF::Material*)AllocateAndZero(allocator, sizeof(glTF::Material) * array_count);
    gltfData.materials_count = array_count;

    for (sizet i = 0; i < array_count; ++i) {
        LoadMaterial(array[i], gltfData.materials[i], allocator);
    }
}

static void LoadTexture(json& jsonData, glTF::Texture& texture, Allocator* allocator) {
    TryLoadInt(jsonData, "sampler", texture.sampler);
    TryLoadInt(jsonData, "source", texture.source);
    TryLoadString(jsonData, "name", texture.name, allocator);
}

static void LoadTextures(json& jsonData, glTF::glTF& gltfData, Allocator* allocator) {
    json array = jsonData["textures"];

    sizet array_count = array.size();
    gltfData.textures = (glTF::Texture*)AllocateAndZero(allocator, sizeof(glTF::Texture) * array_count);
    gltfData.textures_count = array_count;

    for (sizet i = 0; i < array_count; ++i) {
        LoadTexture(array[i], gltfData.textures[i], allocator);
    }
}

static void LoadImage(json& jsonData, glTF::Image& image, Allocator* allocator) {
    TryLoadInt(jsonData, "bufferView", image.buffer_view);
    TryLoadString(jsonData, "mimeType", image.mime_type, allocator);
    TryLoadString(jsonData, "uri", image.uri, allocator);
}

static void LoadImages(json& jsonData, glTF::glTF& gltfData, Allocator* allocator) {
    json array = jsonData["images"];

    sizet array_count = array.size();
    gltfData.images = (glTF::Image*)AllocateAndZero(allocator, sizeof(glTF::Image) * array_count);
    gltfData.images_count = array_count;

    for (sizet i = 0; i < array_count; ++i) {
        LoadImage(array[i], gltfData.images[i], allocator);
    }
}

static void LoadSampler(json& jsonData, glTF::Sampler& sampler, Allocator* allocator) {
    TryLoadInt(jsonData, "magFilter", sampler.mag_filter);
    TryLoadInt(jsonData, "minFilter", sampler.min_filter);
    TryLoadInt(jsonData, "wrapS", sampler.wrap_s);
    TryLoadInt(jsonData, "wrapT", sampler.wrap_t);
}

static void LoadSamplers(json& jsonData, glTF::glTF& gltfData, Allocator* allocator) {
    json array = jsonData["samplers"];

    sizet array_count = array.size();
    gltfData.samplers = (glTF::Sampler*)AllocateAndZero(allocator, sizeof(glTF::Sampler) * array_count);
    gltfData.samplers_count = array_count;

    for (sizet i = 0; i < array_count; ++i) {
        LoadSampler(array[i], gltfData.samplers[i], allocator);
    }
}

static void LoadSkin(json& jsonData, glTF::Skin& skin, Allocator* allocator) {
    TryLoadInt(jsonData, "skeleton", skin.skeleton_root_node_index);
    TryLoadInt(jsonData, "inverseBindMatrices", skin.inverse_bind_matrices_buffer_index);
    TryLoadIntArray(jsonData, "joints", skin.joints_count, &skin.joints, allocator);
}

static void LoadSkins(json& jsonData, glTF::glTF& gltfData, Allocator* allocator) {
    json array = jsonData["skins"];

    sizet array_count = array.size();
    gltfData.skins = (glTF::Skin*)AllocateAndZero(allocator, sizeof(glTF::Skin) * array_count);
    gltfData.skins_count = array_count;

    for (sizet i = 0; i < array_count; ++i) {
        LoadSkin(array[i], gltfData.skins[i], allocator);
    }
}

static void LoadAnimation(json& jsonData, glTF::Animation& animation, Allocator* allocator) {

    json jsonArray = jsonData.at("samplers");
    if (jsonArray.is_array()) {
        sizet count = jsonArray.size();

        glTF::AnimationSampler* values = (glTF::AnimationSampler*)AllocateAndZero(allocator, sizeof(glTF::AnimationSampler) * count);

        for (sizet i = 0; i < count; ++i) {
            json element = jsonArray.at(i);
            glTF::AnimationSampler& sampler = values[i];

            TryLoadInt(element, "input", sampler.input_keyframe_buffer_index);
            TryLoadInt(element, "output", sampler.output_keyframe_buffer_index);

            std::string value = element.value("interpolation", "");
            if (value == "LINEAR") {
                sampler.interpolation = glTF::AnimationSampler::Linear;
            }
            else if (value == "STEP") {
                sampler.interpolation = glTF::AnimationSampler::Step;
            }
            else if (value == "CUBICSPLINE") {
                sampler.interpolation = glTF::AnimationSampler::CubicSpline;
            }
            else {
                sampler.interpolation = glTF::AnimationSampler::Linear;
            }
        }

        animation.samplers = values;
        animation.samplers_count = count;
    }

    jsonArray = jsonData.at("channels");
    if (jsonArray.is_array()) {
        sizet count = jsonArray.size();

        glTF::AnimationChannel* values = (glTF::AnimationChannel*)AllocateAndZero(allocator, sizeof(glTF::AnimationChannel) * count);

        for (sizet i = 0; i < count; ++i) {
            json element = jsonArray.at(i);
            glTF::AnimationChannel& channel = values[i];

            TryLoadInt(element, "sampler", channel.sampler);
            json target = element.at("target");
            TryLoadInt(target, "node", channel.target_node);

            std::string target_path = target.value("path", "");
            if (target_path == "scale") {
                channel.target_type = glTF::AnimationChannel::Scale;
            }
            else if (target_path == "rotation") {
                channel.target_type = glTF::AnimationChannel::Rotation;
            }
            else if (target_path == "translation") {
                channel.target_type = glTF::AnimationChannel::Translation;
            }
            else if (target_path == "weights") {
                channel.target_type = glTF::AnimationChannel::Weights;
            }
            else {
                GASSERTM(false, "Error parsing target path %s\n", target_path.c_str());
                channel.target_type = glTF::AnimationChannel::Count;
            }
        }

        animation.channels = values;
        animation.channels_count = count;
    }
}

static void LoadAnimations(json& jsonData, glTF::glTF& gltfData, Allocator* allocator) {
    json array = jsonData["animations"];

    sizet array_count = array.size();
    gltfData.animations = (glTF::Animation*)AllocateAndZero(allocator, sizeof(glTF::Animation) * array_count);
    gltfData.animations_count = array_count;

    for (sizet i = 0; i < array_count; ++i) {
        LoadAnimation(array[i], gltfData.animations[i], allocator);
    }
}

glTF::glTF GltfLoadFile(cstring file_path) {
    glTF::glTF result{ };

    if (!FileExists(file_path)) {
        GPrint("Error: file %s does not exists.\n", file_path);
        return result;
    }

    Allocator* heapAllocator = &MemoryService::Instance()->systemAllocator;

    FileReadResult read_result = FileReadText(file_path, heapAllocator);

    json gltfData = json::parse(read_result.data);

    result.allocator.Init(GMega(2));
    Allocator* allocator = &result.allocator;

    for (auto properties : gltfData.items()) {
        if (properties.key() == "asset") {
            LoadAsset(gltfData, result.asset, allocator);
        }
        else if (properties.key() == "scene") {
            TryLoadInt(gltfData, "scene", result.scene);
        }
        else if (properties.key() == "scenes") {
            load_scenes(gltfData, result, allocator);
        }
        else if (properties.key() == "buffers") {
            LoadBuffers(gltfData, result, allocator);
        }
        else if (properties.key() == "bufferViews") {
            LoadBufferViews(gltfData, result, allocator);
        }
        else if (properties.key() == "nodes") {
            LoadNodes(gltfData, result, allocator);
        }
        else if (properties.key() == "meshes") {
            LoadMeshes(gltfData, result, allocator);
        }
        else if (properties.key() == "accessors") {
            LoadAccessors(gltfData, result, allocator);
        }
        else if (properties.key() == "materials") {
            LoadMaterials(gltfData, result, allocator);
        }
        else if (properties.key() == "textures") {
            LoadTextures(gltfData, result, allocator);
        }
        else if (properties.key() == "images") {
            LoadImages(gltfData, result, allocator);
        }
        else if (properties.key() == "samplers") {
            LoadSamplers(gltfData, result, allocator);
        }
        else if (properties.key() == "skins") {
            LoadSkins(gltfData, result, allocator);
        }
        else if (properties.key() == "animations") {
            LoadAnimations(gltfData, result, allocator);
        }
    }

    heapAllocator->Deallocate(read_result.data);

    return result;
}

void GltfFree(glTF::glTF& scene) {
    scene.allocator.Shutdown();
}

i32 GltfGetAttributeAccessorIndex(glTF::MeshPrimitive::Attribute* attributes, u32 attributeCount, cstring attribute_name) {
    for (u32 index = 0; index < attributeCount; ++index) {
        glTF::MeshPrimitive::Attribute& attribute = attributes[index];
        if (strcmp(attribute.key.data, attribute_name) == 0) {
            return attribute.accessor_index;
        }
    }

    return -1;
}


i32 glTF::GetDataOffset(i32 accessorOffset, i32 bufferViewOffset) {

    i32 byte_offset = bufferViewOffset == INVALID_INT_VALUE ? 0 : bufferViewOffset;
    byte_offset += accessorOffset == INVALID_INT_VALUE ? 0 : accessorOffset;
    return byte_offset;
}
