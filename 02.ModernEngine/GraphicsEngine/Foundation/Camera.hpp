#pragma once

#include "Platform.hpp"

#include "../external/cglm/struct/mat4.h"

//
// Camera struct - can be both perspective and orthographic.
//
struct Camera
{

    void InitPerpective(f32 near_plane, f32 far_plane, f32 fov_y, f32 aspectRatio);
    void InitOrthographic(f32 near_plane, f32 far_plane, f32 viewportWidth, f32 viewportHeight, f32 zoom);

    void Reset();

    void SetViewportSize(f32 width, f32 height);
    void SetZoom(f32 zoom);
    void SetAspectRatio(f32 aspectRatio);
    void SetFovY(f32 fov_y);

    void Update();
    void Rotate(f32 delta_pitch, f32 delta_yaw);

    void CalculateProjectionMatrix();
    void CalculateViewProjection();

    // Project/unproject
    vec3s Unproject(const vec3s& screen_coordinates);

    // Unproject by inverting the y of the screen coordinate.
    vec3s UnprojectInvertedY(const vec3s& screen_coordinates);

    void GetProjectionOrtho2d(mat4& out_matrix);

    static void YawPitchFromDirection(const vec3s& direction, f32& yaw, f32& pitch);

    mat4s view;
    mat4s projection;
    mat4s viewProjection;

    vec3s position;
    vec3s right;
    vec3s direction;
    vec3s up;

    f32 yaw;
    f32 pitch;

    f32 nearPlane;
    f32 farPlane;

    f32 fieldOfViewY;
    f32 aspectRatio;

    f32 zoom;
    f32 viewportWidth;
    f32 viewportHeight;

    bool perspective;
    bool updateProjection;

}; // struct Camera

