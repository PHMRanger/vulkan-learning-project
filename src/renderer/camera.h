#ifndef RENDERER_CAMERA_H
#define RENDERER_CAMERA_H

#include <linmath.h>

struct FPSCamera {
    float fov;
    float aspect;
    float near;
    float far;
};

void create_fpscamera(FPSCamera *camera, float fov, float aspect, float near, float far)
{
    camera->fov = fov;
    camera->aspect = aspect;
    camera->near = near;
    camera->far = far;
}

void fpscamera_rotation(FPSCamera *camera, float xrad, float yrad, quat rot)
{
    quat xrot;
    quat zrot;

    quat_rotate(xrot,  yrad, vec3{1, 0, 0});
    quat_rotate(zrot, -xrad, vec3{0, 0, 1});

    quat_mul(rot, zrot, xrot);
}

void fpscamera_view(FPSCamera *camera, vec3 pos, quat rot, mat4x4 view)
{
    mat4x4 proj;
    mat4x4_perspective(proj, camera->fov, camera->aspect, camera->near, camera->far);

    mat4x4 trans;
    mat4x4_translate(trans, -pos[0], -pos[1], -pos[2]);

    quat conj; 
    quat_conj(conj, rot);
    mat4x4_from_quat(view, conj);

    mat4x4_mul(view, proj, view);
    mat4x4_mul(view, view, trans);
}

#endif