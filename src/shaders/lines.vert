#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 0) out vec3 fragColor;

layout( push_constant ) uniform ModelViewProjection {
    mat4 mvp;
} mvp;

void main() {
    gl_Position = mvp.mvp * vec4(inPosition, 1.0);
//  fragColor = vec3(.008, .008, .008);
    fragColor = normalize(gl_Position.xyz);
}