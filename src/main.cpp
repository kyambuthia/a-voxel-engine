// a-voxel-engine — portable voxel engine entry point.
//
// Uses SDL3 for windowing/input/lifecycle and an OpenGL renderer (3.3 core on
// desktop, ES 3.0 on Android) behind the portable Renderer interface. The
// renderer can be swapped for a Vulkan backend later without touching this
// file's game-loop logic.

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "render/renderer.h"

namespace {

// Orbit-camera state (drag to look, wheel to zoom).
double gYaw = 0.0;
double gPitch = -0.25;
double gDist = 4.0;
bool gDragging = false;

int gWindowWidth = 1280;
int gWindowHeight = 720;
bool gSuspended = false;  // minimized / (Android) backgrounded

glm::vec3 orbitEye() {
    const float cy = static_cast<float>(std::cos(gPitch));
    const float sy = static_cast<float>(std::sin(gPitch));
    const float cyaw = static_cast<float>(std::cos(gYaw));
    const float syaw = static_cast<float>(std::sin(gYaw));
    return glm::vec3(gDist * cy * syaw, gDist * sy, gDist * cy * cyaw);
}

glm::mat4 buildViewProj() {
    const float aspect =
        (gWindowHeight > 0) ? static_cast<float>(gWindowWidth) /
                                  static_cast<float>(gWindowHeight)
                            : 1.0f;
    const glm::mat4 view =
        glm::lookAt(orbitEye(), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::mat4 proj =
        glm::perspective(glm::radians(60.0f), aspect, 0.1f, 100.0f);
    return proj * view;
}

}  // namespace

int main(int argc, char* argv[]) {
    // Optional smoke-test arg: run for at most this many frames then exit.
    long maxFrames = 0;
    if (argc >= 2) {
        maxFrames = std::atol(argv[1]);
    }

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    // --- GL context hints (must be set before SDL_CreateWindow) ------------
    // Compatibility gate: desktop needs OpenGL 3.3 core, Android needs GLES 3.0.
#if defined(__ANDROID__)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
#endif
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

    SDL_Window* window = SDL_CreateWindow(
        "a-voxel-engine", gWindowWidth, gWindowHeight,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (window == nullptr) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    int pixelW = 0, pixelH = 0;
    SDL_GetWindowSizeInPixels(window, &pixelW, &pixelH);
    gWindowWidth = pixelW;
    gWindowHeight = pixelH;

    voxel::RendererCreateInfo info;
    info.window = window;
    info.width = pixelW;
    info.height = pixelH;
    info.vsync = true;
    info.debug = false;

    auto renderer = voxel::createRenderer(voxel::RendererBackend::OpenGL, info);
    if (!renderer || !renderer->initialize()) {
        std::fprintf(stderr,
                     "Unsupported graphics: could not create an OpenGL context. "
                     "Desktop needs OpenGL 3.3+; Android needs OpenGL ES 3.0+.\n");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    bool running = true;
    Uint64 prevTicks = SDL_GetTicks();
    long frameCount = 0;

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_EVENT_QUIT:
                    running = false;
                    break;

                case SDL_EVENT_KEY_DOWN:
                    if (event.key.key == SDLK_ESCAPE) {
                        running = false;
                    }
                    break;

                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        gDragging = true;
                    }
                    break;
                case SDL_EVENT_MOUSE_BUTTON_UP:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        gDragging = false;
                    }
                    break;
                case SDL_EVENT_MOUSE_MOTION:
                    if (gDragging) {
                        gYaw -= event.motion.xrel * 0.01;
                        gPitch += event.motion.yrel * 0.01;
                        gPitch =
                            std::clamp(gPitch, -1.5, 1.5);  // -86°..+86°
                    }
                    break;
                case SDL_EVENT_MOUSE_WHEEL:
                    gDist *= (event.wheel.y > 0.0f) ? 0.9 : 1.1;
                    gDist = std::clamp(gDist, 1.5, 30.0);
                    break;

                case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                    gWindowWidth = event.window.data1;
                    gWindowHeight = event.window.data2;
                    renderer->onResize(gWindowWidth, gWindowHeight);
                    break;

                case SDL_EVENT_WINDOW_MINIMIZED:
                    gSuspended = true;
                    renderer->onPause();
                    break;
                case SDL_EVENT_WINDOW_RESTORED:
                    if (gSuspended) {
                        gSuspended = false;
                        renderer->onResume();
                    }
                    break;
#if defined(__ANDROID__)
                case SDL_EVENT_WINDOW_FOCUS_LOST:
                    gSuspended = true;
                    renderer->onPause();
                    break;
                case SDL_EVENT_WINDOW_FOCUS_GAINED:
                    if (gSuspended) {
                        gSuspended = false;
                        renderer->onResume();
                    }
                    break;
#endif
                default:
                    break;
            }
        }

        const Uint64 now = SDL_GetTicks();
        const double deltaSeconds =
            static_cast<double>(now - prevTicks) / 1000.0;
        prevTicks = now;

        if (gSuspended) {
            SDL_Delay(16);
            continue;
        }

        if (renderer->beginFrame()) {
            voxel::RenderCamera camera;
            camera.viewProj = buildViewProj();
            camera.eye = orbitEye();
            renderer->renderFrame(camera, deltaSeconds);
            renderer->endFrame();
        }

        ++frameCount;
        if (maxFrames > 0 && frameCount >= maxFrames) {
            running = false;
        }
    }

    renderer->shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
