cmake_minimum_required(VERSION 3.24)
project(Vulkan1.4-Intro)

set(CMAKE_CXX_STANDARD 20)

include(FetchContent)

message(STATUS "")
message(STATUS "╔════════════════════════════════════════════════════════════╗")
message(STATUS "║           Fetching Dependencies                            ║")
message(STATUS "╚════════════════════════════════════════════════════════════╝")
message(STATUS "")

# SDL3
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.2
    GIT_SHALLOW TRUE
)
message(STATUS "⬇  Downloading SDL3 (release-3.4.2)...")
FetchContent_MakeAvailable(SDL3)
message(STATUS "✔  SDL3 ready")
message(STATUS "")

# GLM
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
    GIT_SHALLOW TRUE
)
message(STATUS "⬇  Downloading GLM (1.0.1)...")
FetchContent_MakeAvailable(glm)
message(STATUS "✔  GLM ready")
message(STATUS "")

# stb
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master
    GIT_SHALLOW TRUE
)
message(STATUS "⬇  Downloading stb (master)...")
FetchContent_MakeAvailable(stb)
message(STATUS "✔  stb ready")
message(STATUS "")

# tinyobjloader
FetchContent_Declare(
    tinyobjloader
    GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
    GIT_TAG v2.0.0rc13
    GIT_SHALLOW TRUE
)
message(STATUS "⬇  Downloading tinyobjloader (v2.0.0rc13)...")
FetchContent_MakeAvailable(tinyobjloader)
message(STATUS "✔  tinyobjloader ready")
message(STATUS "")

# vk-bootstrap
FetchContent_Declare(
    vk-bootstrap
    GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap.git
    GIT_TAG v1.4.343
    GIT_SHALLOW TRUE
)
message(STATUS "⬇  Downloading vk-bootstrap (v1.4.343)...")
FetchContent_MakeAvailable(vk-bootstrap)
message(STATUS "✔  vk-bootstrap ready")
message(STATUS "")

# VMA (Vulkan Memory Allocator)
FetchContent_Declare(
    VulkanMemoryAllocator
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG v3.2.1
    GIT_SHALLOW TRUE
)
message(STATUS "⬇  Downloading VMA (v3.2.1)...")
FetchContent_MakeAvailable(VulkanMemoryAllocator)
message(STATUS "✔  VMA ready")
message(STATUS "")

message(STATUS "╔════════════════════════════════════════════════════════════╗")
message(STATUS "║           All Dependencies Ready                           ║")
message(STATUS "╚════════════════════════════════════════════════════════════╝")
message(STATUS "")

# Find Vulkan
message(STATUS "🔍 Finding Vulkan SDK...")
find_package(Vulkan REQUIRED)
message(STATUS "✔  Vulkan ${Vulkan_VERSION} found")
message(STATUS "")

add_executable(VulkanIntro
    main.cpp
    Application.cpp
    Application.hpp
    Defines.h
    BasicServices/Log.cpp
    BasicServices/Log.h
    BasicServices/Platform.h
    BasicServices/FileWriter.cpp
    BasicServices/FileWriter.h
    Engine/Engine.cpp
    Engine/Engine.hpp
    Engine/Renderer/IRenderer.hpp
    Engine/Renderer/Buffer.hpp
    Engine/Renderer/Buffer.cpp
    Engine/Renderer/Image.hpp
    Engine/Renderer/Image.cpp
    Engine/Renderer/Model.hpp
    Engine/Renderer/Model.cpp
    Engine/Renderer/ModelLoader.hpp
    Engine/Renderer/ModelLoader.cpp
    Engine/Renderer/Texture.hpp
    Engine/Renderer/Texture.cpp
    Engine/Renderer/TextureLoader.hpp
    Engine/Renderer/TextureLoader.cpp
    Engine/Renderer/ShaderLoader.hpp
    Engine/Renderer/ShaderLoader.cpp
    Engine/Renderer/ImmediateSubmit.hpp
    Engine/Renderer/ImmediateSubmit.cpp
    Engine/IScene.hpp
    Steps/Step00_Setup/Step00Renderer.cpp
    Steps/Step00_Setup/Step00Renderer.hpp
    Steps/Step00_Setup/Step00Scene.hpp
    Steps/Step01_BasicPipeline/Step01Renderer.cpp
    Steps/Step01_BasicPipeline/Step01Renderer.hpp
    Steps/Step01_BasicPipeline/Step01Scene.hpp
    Steps/Step02_Triangle/Step02Renderer.cpp
    Steps/Step02_Triangle/Step02Renderer.hpp
    Steps/Step02_Triangle/Step02Scene.hpp
    Steps/Step03_SwapchainRecreation/Step03Renderer.cpp
    Steps/Step03_SwapchainRecreation/Step03Renderer.hpp
    Steps/Step03_SwapchainRecreation/Step03Scene.hpp
    Steps/Step04_VertexBuffer/Step04Renderer.cpp
    Steps/Step04_VertexBuffer/Step04Renderer.hpp
    Steps/Step04_VertexBuffer/Step04Scene.hpp
    Steps/Step05_UniformBuffer/Step05Renderer.cpp
    Steps/Step05_UniformBuffer/Step05Renderer.hpp
    Steps/Step05_UniformBuffer/Step05Scene.cpp
    Steps/Step05_UniformBuffer/Step05Scene.hpp
    Steps/Step06_Textures/Step06Renderer.cpp
    Steps/Step06_Textures/Step06Renderer.hpp
    Steps/Step06_Textures/Step06Scene.cpp
    Steps/Step06_Textures/Step06Scene.hpp
    Steps/Step07_Depth/Step07Renderer.cpp
    Steps/Step07_Depth/Step07Renderer.hpp
    Steps/Step07_Depth/Step07Scene.cpp
    Steps/Step07_Depth/Step07Scene.hpp
    Steps/Step08_Modele3D/Step08Renderer.cpp
    Steps/Step08_Modele3D/Step08Renderer.hpp
    Steps/Step08_Modele3D/Step08Scene.cpp
    Steps/Step08_Modele3D/Step08Scene.hpp
    Steps/Step09_MultisamplingMipMaps/Step09Renderer.cpp
    Steps/Step09_MultisamplingMipMaps/Step09Renderer.hpp
    Steps/Step09_MultisamplingMipMaps/Step09Scene.cpp
    Steps/Step09_MultisamplingMipMaps/Step09Scene.hpp
    Steps/Step10_ComputeShaderParticles/Step10Renderer.cpp
    Steps/Step10_ComputeShaderParticles/Step10Renderer.hpp
    Steps/Step10_ComputeShaderParticles/Step10Scene.cpp
    Steps/Step10_ComputeShaderParticles/Step10Scene.hpp
)

# Platform-specific sources
if(WIN32)
    target_sources(VulkanIntro PRIVATE BasicServices/Platform_Win.cpp)
else()
    target_sources(VulkanIntro PRIVATE BasicServices/Platform_Linux.cpp)
endif()

target_link_libraries(VulkanIntro
    PRIVATE
        SDL3::SDL3
        Vulkan::Vulkan
        glm::glm
        tinyobjloader
        vk-bootstrap::vk-bootstrap
        GPUOpen::VulkanMemoryAllocator
)

# stb is header-only, add include path
target_include_directories(VulkanIntro PRIVATE ${stb_SOURCE_DIR})

# Shader compilation — Slang → SPIR-V
find_program(SLANGC slangc REQUIRED)   # /usr/local/bin/slangc on this system
set(SHADER_SRC "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/01.BasicPipeline.slang")
set(SHADER_SPV "${CMAKE_CURRENT_BINARY_DIR}/assets/shaders/01.BasicPipeline.spv")
add_custom_command(
    OUTPUT  ${SHADER_SPV}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/assets/shaders"
    COMMAND ${SLANGC} -target spirv -profile spirv_1_4 -emit-spirv-directly
            -entry vertMain -entry fragMain ${SHADER_SRC} -o ${SHADER_SPV}
    DEPENDS ${SHADER_SRC}
    COMMENT "Compiling 01.BasicPipeline.slang → SPIR-V"
)
set(SHADER04_SRC "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/04.VertexBuffer.slang")
set(SHADER04_SPV "${CMAKE_CURRENT_BINARY_DIR}/assets/shaders/04.VertexBuffer.spv")
add_custom_command(
    OUTPUT  ${SHADER04_SPV}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/assets/shaders"
    COMMAND ${SLANGC} -target spirv -profile spirv_1_4 -emit-spirv-directly
            -entry vertMain -entry fragMain ${SHADER04_SRC} -o ${SHADER04_SPV}
    DEPENDS ${SHADER04_SRC}
    COMMENT "Compiling 04.VertexBuffer.slang → SPIR-V"
)
set(SHADER05_SRC "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/05.UniformBuffer.slang")
set(SHADER05_SPV "${CMAKE_CURRENT_BINARY_DIR}/assets/shaders/05.UniformBuffer.spv")
add_custom_command(
    OUTPUT  ${SHADER05_SPV}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/assets/shaders"
    COMMAND ${SLANGC} -target spirv -profile spirv_1_4 -emit-spirv-directly
            -entry vertMain -entry fragMain ${SHADER05_SRC} -o ${SHADER05_SPV}
    DEPENDS ${SHADER05_SRC}
    COMMENT "Compiling 05.UniformBuffer.slang → SPIR-V"
)
set(SHADER06_SRC "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/06.Textures.slang")
set(SHADER06_SPV "${CMAKE_CURRENT_BINARY_DIR}/assets/shaders/06.Textures.spv")
add_custom_command(
    OUTPUT  ${SHADER06_SPV}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/assets/shaders"
    COMMAND ${SLANGC} -target spirv -profile spirv_1_4 -emit-spirv-directly
            -entry vertMain -entry fragMain ${SHADER06_SRC} -o ${SHADER06_SPV}
    DEPENDS ${SHADER06_SRC}
    COMMENT "Compiling 06.Textures.slang → SPIR-V"
)
set(SHADER07_SRC "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/07.Depth.slang")
set(SHADER07_SPV "${CMAKE_CURRENT_BINARY_DIR}/assets/shaders/07.Depth.spv")
add_custom_command(
    OUTPUT  ${SHADER07_SPV}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/assets/shaders"
    COMMAND ${SLANGC} -target spirv -profile spirv_1_4 -emit-spirv-directly
            -entry vertMain -entry fragMain ${SHADER07_SRC} -o ${SHADER07_SPV}
    DEPENDS ${SHADER07_SRC}
    COMMENT "Compiling 07.Depth.slang → SPIR-V"
)
set(SHADER08_SRC "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/08.Modele3D.slang")
set(SHADER08_SPV "${CMAKE_CURRENT_BINARY_DIR}/assets/shaders/08.Modele3D.spv")
add_custom_command(
    OUTPUT  ${SHADER08_SPV}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/assets/shaders"
    COMMAND ${SLANGC} -target spirv -profile spirv_1_4 -emit-spirv-directly
            -entry vertMain -entry fragMain ${SHADER08_SRC} -o ${SHADER08_SPV}
    DEPENDS ${SHADER08_SRC}
    COMMENT "Compiling 08.Modele3D.slang → SPIR-V"
)
set(SHADER09_SRC "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/09.Multisampling.slang")
set(SHADER09_SPV "${CMAKE_CURRENT_BINARY_DIR}/assets/shaders/09.Multisampling.spv")
add_custom_command(
    OUTPUT  ${SHADER09_SPV}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/assets/shaders"
    COMMAND ${SLANGC} -target spirv -profile spirv_1_4 -emit-spirv-directly
            -entry vertMain -entry fragMain ${SHADER09_SRC} -o ${SHADER09_SPV}
    DEPENDS ${SHADER09_SRC}
    COMMENT "Compiling 09.Multisampling.slang → SPIR-V"
)
set(SHADER10_SRC "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/10.ComputeParticles.slang")
set(SHADER10G_SPV "${CMAKE_CURRENT_BINARY_DIR}/assets/shaders/10.ComputeParticles.graphics.spv")
set(SHADER10C_SPV "${CMAKE_CURRENT_BINARY_DIR}/assets/shaders/10.ComputeParticles.compute.spv")
add_custom_command(
    OUTPUT  ${SHADER10G_SPV}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/assets/shaders"
    COMMAND ${SLANGC} -target spirv -profile spirv_1_4 -emit-spirv-directly
            -entry vertMain -entry fragMain ${SHADER10_SRC} -o ${SHADER10G_SPV}
    DEPENDS ${SHADER10_SRC}
    COMMENT "Compiling 10.ComputeParticles.slang → graphics SPIR-V"
)
add_custom_command(
    OUTPUT  ${SHADER10C_SPV}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/assets/shaders"
    COMMAND ${SLANGC} -target spirv -profile spirv_1_4 -emit-spirv-directly
            -entry compMain ${SHADER10_SRC} -o ${SHADER10C_SPV}
    DEPENDS ${SHADER10_SRC}
    COMMENT "Compiling 10.ComputeParticles.slang → compute SPIR-V"
)
add_custom_target(Shaders DEPENDS ${SHADER_SPV} ${SHADER04_SPV} ${SHADER05_SPV} ${SHADER06_SPV} ${SHADER07_SPV} ${SHADER08_SPV} ${SHADER09_SPV} ${SHADER10G_SPV} ${SHADER10C_SPV})
add_dependencies(VulkanIntro Shaders)

# Copy assets from source to build directory on every build
add_custom_command(TARGET VulkanIntro POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/textures"
            "${CMAKE_CURRENT_BINARY_DIR}/assets/textures"
    COMMENT "Copying assets/textures → build directory"
)
add_custom_command(TARGET VulkanIntro POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/models"
            "${CMAKE_CURRENT_BINARY_DIR}/assets/models"
    COMMENT "Copying assets/models → build directory"
)

# Set rpath for macOS to find Vulkan and SDL3 libraries at runtime
if(APPLE)
    set_target_properties(VulkanIntro PROPERTIES
        BUILD_RPATH "/usr/local/lib;${CMAKE_BINARY_DIR}/_deps/sdl3-build"
        INSTALL_RPATH "/usr/local/lib"
    )
endif()
