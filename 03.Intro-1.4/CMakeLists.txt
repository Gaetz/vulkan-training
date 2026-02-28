cmake_minimum_required(VERSION 3.24)
project(Vulkan1.4-Intro)

set(CMAKE_CXX_STANDARD 20)

# Download SDL3
include(FetchContent)
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.2
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(SDL3)

# Find Vulkan
find_package(Vulkan REQUIRED)

add_executable(VulkanIntro main.cpp)

target_link_libraries(VulkanIntro
    PRIVATE
        SDL3::SDL3
        Vulkan::Vulkan
)

# Set rpath for macOS to find Vulkan and SDL3 libraries at runtime
if(APPLE)
    set_target_properties(VulkanIntro PROPERTIES
        BUILD_RPATH "/usr/local/lib;${CMAKE_BINARY_DIR}/_deps/sdl3-build"
        INSTALL_RPATH "/usr/local/lib"
    )
endif()
