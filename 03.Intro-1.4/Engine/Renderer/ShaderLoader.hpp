#pragma once

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include <string>

// Loads a SPIR-V binary from disk and wraps it in a VkShaderModule.
// Returns vk::raii::ShaderModule{nullptr} and logs an error on failure.
vk::raii::ShaderModule loadShaderModule(vk::raii::Device& device,
                                         const std::string& path);
