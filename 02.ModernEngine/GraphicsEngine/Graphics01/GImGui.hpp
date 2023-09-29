#pragma once

#include <stdint.h>

#include "Service.hpp"

struct ImDrawData;


struct GpuDevice;
struct CommandBuffer;
struct TextureHandle;

//
//
enum ImGuiStyles
{
    Default = 0,
    GreenBlue,
    DarkRed,
    DarkGold
}; // enum ImGuiStyles

//
//
struct ImGuiServiceConfiguration
{

    GpuDevice* gpu;
    void* window_handle;

}; // struct ImGuiServiceConfiguration

//
//
struct ImGuiService : public Service
{

    G_DECLARE_SERVICE(ImGuiService);

    void Init(void* configuration) override;
    void Shutdown() override;

    void new_frame();
    void render(CommandBuffer& commands);

    // Removes the Texture from the Cache and destroy the associated Descriptor Set.
    void remove_cached_texture(TextureHandle& texture);

    void set_style(ImGuiStyles style);

    GpuDevice* gpu;

    static constexpr cstring k_name = "g_imgui_service";

}; // ImGuiService

// File Dialog /////////////////////////////////////////////////////////

/*bool                                imgui_file_dialog_open( const char* button_name, const char* path, const char* extension );
const char*                         imgui_file_dialog_get_filename();

bool                                imgui_path_dialog_open( const char* button_name, const char* path );
const char*                         imgui_path_dialog_get_path();*/

// Application Log /////////////////////////////////////////////////////

void imgui_log_init();
void imgui_log_shutdown();

void imgui_log_draw();

// FPS graph ///////////////////////////////////////////////////
void imgui_fps_init();
void imgui_fps_shutdown();
void imgui_fps_add(f32 dt);
void imgui_fps_draw();

