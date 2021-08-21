#include "renderer/renderer.h"
#include "renderer/debug.h"
#include "octree/octree.h"

#include "math/math.h"

double g_xpos, g_ypos;
vec3 camera_position;
vec2 axis;

bool freeze_time = false;

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    static bool cursor_enabled = false;
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        cursor_enabled = !cursor_enabled;

        if (cursor_enabled) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        } else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
    }

    if (action == GLFW_PRESS) {
        switch(key) {
        case GLFW_KEY_W:
            axis[1] = 1;
            break;
        case GLFW_KEY_S:
            axis[1] = -1;
            break;
        case GLFW_KEY_D:
            axis[0] = 1;
            break;
        case GLFW_KEY_A:
            axis[0] = -1;
            break;
        case GLFW_KEY_SPACE:
            freeze_time = true;
            break;
        }
    }

    if (action == GLFW_RELEASE) {
        switch(key) {
        case GLFW_KEY_W:
        case GLFW_KEY_S:
            axis[1] = 0;
            break;
        case GLFW_KEY_D:
        case GLFW_KEY_A:
            axis[0] = 0;
            break;
        }
    }
}

static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
{
    g_xpos = xpos;
    g_ypos = ypos;
}

int main() {
    Octree::Octree oct;
    Octree::Init(&oct);

//  Draw debug grid
    vec3 lines[32000];

    vec3 glines[32000];
    size_t index = 0;
    int32_t grid_lines = 7;
    for (int i = 0; i < grid_lines; ++i) {
        vec3 a = {grid_lines / 2,  i - grid_lines / 2, 0};
        vec3 b = {-grid_lines / 2, i - grid_lines / 2, 0};
        memcpy(glines[index++], a, sizeof(vec3));
        memcpy(glines[index++], b, sizeof(vec3));
    }
    for (int i = 0; i < grid_lines; ++i) {
        vec3 a = {i - grid_lines / 2, grid_lines / 2, 0};
        vec3 b = {i - grid_lines / 2, -grid_lines / 2, 0};
        memcpy(glines[index++], a, sizeof(vec3));
        memcpy(glines[index++], b, sizeof(vec3));
    }
    size_t gcount = index;

    Renderer renderer;
    renderer_init(&renderer);

    DebugRenderer debug;
    debug_renderer_init(&renderer, &debug);

    glfwSetKeyCallback(renderer.window, key_callback);
    glfwSetCursorPosCallback(renderer.window, cursor_position_callback);

    Material matoctree;
    create_material(&renderer, &matoctree);
    material_descriptors(&renderer, &matoctree);

    UniformBuffer uniform;
    create_uniform_buffer(&renderer, &uniform, sizeof(float) * 16);

    material_update_descriptors(&renderer, &matoctree, &uniform);

    VertexBuffer grid_mesh;
    create_vertex_buffer(&renderer, &grid_mesh, gcount * sizeof(vec3));
    fill_vertex_buffer(&renderer, &grid_mesh, (void *)glines, gcount * sizeof(vec3));

    FPSCamera camera;
    create_fpscamera(&camera, to_radians(90.f), 4.f / 3.f, .01f, 1000.f);

    vec3 pmin, pmax;
    uint32_t frame_counter = 0;
    double time_since_last = glfwGetTime();
    while (!glfwWindowShouldClose(renderer.window)) {
        glfwPollEvents();

        // Draw a frame.
        VkCommandBuffer cmdbuffer;
        begin_frame(&renderer, &cmdbuffer);

        quat rot;
        fpscamera_rotation(&camera, to_radians(0.f) + g_xpos / 1000, to_radians(90.f) - g_ypos / 1000, rot);

        vec3 delta;
        quat_mul_vec3(delta, rot, vec3{axis[0], 0, -axis[1]});

        if (freeze_time) {
            float tmin, tmax;
            vec3 cam_dir;
            quat_mul_vec3(cam_dir, rot, vec3{0, 0, -1});

            // fmt::print("truth: {}\n", intersect_cube(camera_position, cam_dir, vec3{-oct.size / 2, -oct.size / 2, -oct.size / 2}, vec3{oct.size / 2, oct.size / 2, oct.size / 2}, &tmin, &tmax));
            intersect_cube(camera_position, cam_dir, vec3{-oct.size / 2, -oct.size / 2, -oct.size / 2}, vec3{oct.size / 2, oct.size / 2, oct.size / 2}, &tmin, &tmax);
            intersect_point(camera_position, cam_dir, tmin, pmin);
            intersect_point(camera_position, cam_dir, tmax, pmax);

            Octree::InsertPoint(&oct, 0, oct.center, pmax, 0, 6);

            fmt::print("{} {} {}\n", pmin[0], pmin[1], pmin[2]);

            freeze_time = false;
        }


        size_t count = 0;
        Octree::DebugLineList(&oct, 0, vec3{0, 0, 0}, lines, 32000, 0, 8, &count);
        for (size_t i = 0; i < count; i += 2) {
            debug_renderer_drawline(&debug, lines[i], lines[i + 1]);
        }

        debug_renderer_drawsphere(&debug, pmin, .1f, 8);
        debug_renderer_drawsphere(&debug, pmax, .1f, 8);

        vec3_scale(delta, delta, 1.f / 1000.f);

        mat4x4 view;
        vec3_add(camera_position, camera_position, delta);
        fpscamera_view(&camera, camera_position, rot, view);

        mat4x4 model;
        mat4x4_translate(model, 0, 0, 0);

        mat4x4 mvp;

        mat4x4_dup(mvp, view);
        mat4x4_mul(mvp, mvp, model);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        vkBeginCommandBuffer(cmdbuffer, &beginInfo);

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderer.pass;
        renderPassInfo.framebuffer = renderer.framebuffers[renderer.frame_index];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = VkExtent2D{WIDTH, HEIGHT};

        VkClearValue clearColor = {.87f, .87f, .87f, 1.0f};
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(cmdbuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        VkBuffer vertexBuffers[] = { grid_mesh.buffer };
        VkDeviceSize offsets[] = {0};
        vkCmdBindPipeline(cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, matoctree.pipeline);

        vkCmdPushConstants(cmdbuffer, matoctree.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, 128, mvp);
        vkCmdBindVertexBuffers(cmdbuffer, 0, 1, vertexBuffers, offsets);
        vkCmdDraw(cmdbuffer, gcount, 1, 0, 0);

        // Draw debug primitives
        if (debug.count) {
            uint32_t debug_count;
            debug_renderer_flush(&renderer, &debug, &debug_count);

            vkCmdPushConstants(cmdbuffer, matoctree.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, 128, mvp);
            vkCmdBindVertexBuffers(cmdbuffer, 0, 1, &debug.buffer.buffer, offsets);
            vkCmdDraw(cmdbuffer, debug_count, 1, 0, 0);
        }

        vkCmdEndRenderPass(cmdbuffer);

        vkEndCommandBuffer(cmdbuffer);

        submit_frame(&renderer, &cmdbuffer);

        frame_counter++;

        if (time_since_last + 1 < glfwGetTime()) {
            time_since_last = glfwGetTime();

            fmt::print("FPS: {}\n", frame_counter);
            frame_counter = 0;
        }
    }

    return 0;
}