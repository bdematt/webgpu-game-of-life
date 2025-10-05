#define WEBGPU_CPP_IMPLEMENTATION
#include "webgpu.hpp"
#include "Life.h"

static constexpr int FPS = 0;
static constexpr bool SIMULATE_INFINITE_LOOP = true;

// Global pointer to access from C callback
static Life* g_life = nullptr;

// Emscripten exposed function, called during window resize
extern "C" {
    EMSCRIPTEN_KEEPALIVE
    void handleResize() {
        if (g_life) {
            g_life->handleResize();
        }
    }
}

int main() {
    try {
        Life life {};
        g_life = &life;
        auto renderLoop = [&life]() {
            life.renderFrame();
        };
        emscripten_set_main_loop_arg(
            [](void* arg) {
                auto* loop = static_cast<decltype(renderLoop)*>(arg);
                (*loop)();
            },
            &renderLoop,
            FPS,
            SIMULATE_INFINITE_LOOP
        );
    } catch(const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}