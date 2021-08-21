#ifndef RENDERER_DEBUG_H
#define RENDERER_DEBUG_H

#include "renderer.h"
#include "assert.h"

struct DebugRenderer {
    uint32_t count;
    VertexBuffer buffer;
    vec3 *verts;
};

static const uint32_t DEFAULT_SIZE = 1024 * 16;

void debug_renderer_init(Renderer *renderer, DebugRenderer *debug)
{
    create_vertex_buffer(renderer, &debug->buffer, DEFAULT_SIZE * sizeof(vec3));
    debug->verts = new vec3[DEFAULT_SIZE];
    debug->count = 0;
}

void debug_renderer_drawline(DebugRenderer *debug, vec3 a, vec3 b)
{
    memcpy(debug->verts[debug->count++], a, sizeof(vec3));
    memcpy(debug->verts[debug->count++], b, sizeof(vec3));
}

void debug_renderer_drawsphere(DebugRenderer *debug, vec3 c0, float s, uint16_t iter)
{
    float astep = 2.f * M_PI / iter;
    float s0, s1;
    vec3 p0, p1;

    for (size_t i = 0; i < iter; i++) {
        s0 = astep * i;
        s1 = astep * (i + 1);

        vec3_add(p0, c0, vec3{cosf(s0) * s, sinf(s0) * s, 0});
        vec3_add(p1, c0, vec3{cosf(s1) * s, sinf(s1) * s, 0});
        debug_renderer_drawline(debug, p0, p1);
    }

    for (size_t i = 0; i < iter; i++) {
        s0 = astep * i;
        s1 = astep * (i + 1);

        vec3_add(p0, c0, vec3{cosf(s0) * s, 0, sinf(s0) * s});
        vec3_add(p1, c0, vec3{cosf(s1) * s, 0, sinf(s1) * s});
        debug_renderer_drawline(debug, p0, p1);
    }

    for (size_t i = 0; i < iter; i++) {
        s0 = astep * i;
        s1 = astep * (i + 1);

        vec3_add(p0, c0, vec3{0, cosf(s0) * s, sinf(s0) * s});
        vec3_add(p1, c0, vec3{0, cosf(s1) * s, sinf(s1) * s});

        debug_renderer_drawline(debug, p0, p1);
    }
}

void debug_renderer_flush(Renderer *renderer, DebugRenderer *debug, uint32_t *count)
{
    assert(debug->count);
    
    *count = debug->count;
    fill_vertex_buffer(renderer, &debug->buffer, (void *)debug->verts, debug->count * sizeof(vec3));
    debug->count = 0;
}

#endif