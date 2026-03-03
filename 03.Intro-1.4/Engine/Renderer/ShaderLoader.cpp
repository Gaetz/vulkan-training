#include "ShaderLoader.hpp"
#include "../../BasicServices/Log.h"

#include <fstream>
#include <vector>

using services::Log;

vk::raii::ShaderModule loadShaderModule(vk::raii::Device& device,
                                         const std::string& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        Log::Error("Failed to open shader file: %s", path.c_str());
        return vk::raii::ShaderModule{nullptr};
    }
    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<uint32_t> code(fileSize / sizeof(uint32_t));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(code.data()), static_cast<std::streamsize>(fileSize));
    return device.createShaderModule(vk::ShaderModuleCreateInfo{
        .codeSize = fileSize,
        .pCode    = code.data(),
    });
}
