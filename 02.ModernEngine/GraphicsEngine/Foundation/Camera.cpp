#include "Camera.hpp"

#include "../external/cglm/struct/cam.h"
#include "../external/cglm/struct/affine.h"
#include "../external/cglm/struct/quat.h"
#include "../external/cglm/struct/project.h"


// Camera ///////////////////////////////////////////////////////////////////////

void Camera::InitPerpective(f32 nearPlane_, f32 farPlane_, f32 fovY, f32 aspectRatio_) {
    perspective = true;

    nearPlane = nearPlane_;
    farPlane = farPlane_;
    fieldOfViewY = fovY;
    aspectRatio = aspectRatio_;

    Reset();
}

void Camera::InitOrthographic(f32 nearPlane_, f32 farPlane_, f32 viewportWidth_, f32 viewportHeight_, f32 zoom_) {
    perspective = false;

    nearPlane = nearPlane_;
    farPlane = farPlane_;

    viewportWidth = viewportWidth_;
    viewportHeight = viewportHeight_;
    zoom = zoom_;

    Reset();
}

void Camera::Reset() {
    position = glms_vec3_zero();
    yaw = 0;
    pitch = 0;
    view = glms_mat4_identity();
    projection = glms_mat4_identity();

    updateProjection = true;
}

void Camera::SetViewportSize(f32 width_, f32 height_) {
    viewportWidth = width_;
    viewportHeight = height_;

    updateProjection = true;
}

void Camera::SetZoom(f32 zoom_) {
    zoom = zoom_;

    updateProjection = true;
}

void Camera::SetAspectRatio(f32 aspectRatio_) {
    aspectRatio = aspectRatio_;

    updateProjection = true;
}

void Camera::SetFovY(f32 fovY_) {
    fieldOfViewY = fovY_;

    updateProjection = true;
}

void Camera::Update() {

    // Left for reference.
    // Calculate rotation from yaw and pitch
    /*direction.x = sinf( ( yaw ) ) * cosf( ( pitch ) );
    direction.y = sinf( ( pitch ) );
    direction.z = cosf( ( yaw ) ) * cosf( ( pitch ) );
    direction = glms_vec3_normalize( direction );

    vec3s center = glms_vec3_sub( position, direction );
    vec3s cup{ 0,1,0 };

    right = glms_cross( cup, direction );
    up = glms_cross( direction, right );

    // Calculate view matrix
    view = glms_lookat( position, center, up );
    */

    // Quaternion based rotation.
    // https://stackoverflow.com/questions/49609654/quaternion-based-first-person-view-camera
    const versors pitchRotation = glms_quat(pitch, 1, 0, 0);
    const versors yawRotation = glms_quat(yaw, 0, 1, 0);
    const versors rotation = glms_quat_normalize(glms_quat_mul(pitchRotation, yawRotation));

    const mat4s translation = glms_translate_make(glms_vec3_scale(position, -1.f));
    view = glms_mat4_mul(glms_quat_mat4(rotation), translation);

    // Update the vectors used for movement
    right = { view.m00, view.m10, view.m20 };
    up = { view.m01, view.m11, view.m21 };
    direction = { view.m02, view.m12, view.m22 };

    if (updateProjection) {
        updateProjection = false;

        CalculateProjectionMatrix();
    }

    // Calculate final view projection matrix
    CalculateViewProjection();
}

void Camera::Rotate(f32 deltaPitch, f32 deltaYaw) {

    pitch += deltaPitch;
    yaw += deltaYaw;
}

void Camera::CalculateProjectionMatrix() {
    if (perspective) {
        projection = glms_perspective(glm_rad(fieldOfViewY), aspectRatio, nearPlane, farPlane);
    }
    else {
        projection = glms_ortho(zoom * -viewportWidth / 2.f, zoom * viewportWidth / 2.f, zoom * -viewportHeight / 2.f, zoom * viewportHeight / 2.f, nearPlane, farPlane);
    }
}

void Camera::CalculateViewProjection() {
    viewProjection = glms_mat4_mul(projection, view);
}

vec3s Camera::Unproject(const vec3s& screenCoordinates) {
    return glms_unproject(screenCoordinates, viewProjection, { 0, 0, viewportWidth, viewportHeight });
}

vec3s Camera::UnprojectInvertedY(const vec3s& screenCoordinates) {
    const vec3s screenCoordinates_y_inv{ screenCoordinates.x, viewportHeight - screenCoordinates.y, screenCoordinates.z };
    return Unproject(screenCoordinates_y_inv);
}

void Camera::GetProjectionOrtho2d(mat4& outMatrix) {
    glm_ortho(0, viewportWidth * zoom, 0, viewportHeight * zoom, -1.f, 1.f, outMatrix);
}

void Camera::YawPitchFromDirection(const vec3s& direction, f32& yaw, f32& pitch) {

    yaw = glm_deg(atan2f(direction.z, direction.x));
    pitch = glm_deg(asinf(direction.y));
}


