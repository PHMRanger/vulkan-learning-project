#include "renderer.h"
#include "octree/octree.h"

#include <linmath.h>

int main() {
    Octree::Octree oct;
    Octree::Init(&oct);

    Octree::SplitNode(&oct, 0);
    Octree::SplitNode(&oct, 1);

    vec3 lines[32000];
    size_t count = Octree::DebugLineList(&oct, 0, lines, 32000, 8);

    fmt::print("lines: {}\n", count);

    for (size_t i = 0; i < count; ++i) {
        fmt::print("{} {} {}", lines[i][0], lines[i][1], lines[i][2]);
    }

    Renderer renderer;
    renderer_init(&renderer);

    VkPipeline pipeline;
    renderer_pipeline(&renderer, &pipeline);

    uint32_t frame_counter = 0;
    double time_since_last = glfwGetTime();
    while (!glfwWindowShouldClose(renderer.window)) {
        glfwPollEvents();
        drawframe(&renderer, pipeline);

        frame_counter++;

        if (time_since_last + 1 < glfwGetTime()) {
            time_since_last = glfwGetTime();

            fmt::print("FPS: {}\n", frame_counter);
            frame_counter = 0;
        }
    }

    return 0;
}