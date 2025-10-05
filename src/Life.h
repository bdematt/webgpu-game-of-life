#pragma once
#include <cstdint>
#include "webgpu.hpp"
#include <chrono>

class Life
{
private:
    // WGPU Context
    struct PingPongBuffers {
        wgpu::Buffer read{};
        wgpu::Buffer write{};
        wgpu::BindGroup readBindGroup{};
        wgpu::BindGroup writeBindGroup{};
        
        void swap() {
            std::swap(read, write);
            std::swap(readBindGroup, writeBindGroup);
        }
    };
    
    wgpu::Instance instance {};
    wgpu::Adapter adapter{nullptr};
    wgpu::Device device{nullptr};
    wgpu::Queue queue{nullptr};
    wgpu::Surface surface{nullptr};
    wgpu::SurfaceConfiguration surfaceConfig{};
    wgpu::RenderPipeline renderPipeline{nullptr};
    wgpu::ComputePipeline simulationPipeline{nullptr};
    wgpu::Buffer vertexBuffer{nullptr};
    wgpu::Buffer uniformBuffer{nullptr};
    PingPongBuffers cellBuffers;
    wgpu::BindGroupLayout bindGroupLayout{nullptr};
    wgpu::BindGroup bindGroup{nullptr};

    // Geometry
    static constexpr float VERTICES[] = {
        -0.8f, -0.8f,
         0.8f, -0.8f,
         0.8f,  0.8f,
        
        -0.8f, -0.8f,
         0.8f,  0.8f,
        -0.8f,  0.8f,
    };
    static constexpr int GRID_SIZE = 256;
    static constexpr int WORKGROUP_SIZE = 8;
    static constexpr float GRID_DIMENSIONS[2] = {
        static_cast<float>(GRID_SIZE), 
        static_cast<float>(GRID_SIZE)
    };

    // Cell State
    static constexpr float UPDATE_INTERVAL_SECONDS = 0.1f;
    std::vector<uint32_t> cellStateArray;
    float accumulatedTime = UPDATE_INTERVAL_SECONDS;
    std::chrono::steady_clock::time_point lastFrameTime;
    uint32_t step = 0;
    
    void requestAdapter();
    void requestDevice();
    void createSurface();
    void configureSurface();
    void createPipelines();
    void createVertexBuffer();
    void createUniformBuffer();
    void createStorageBuffers();
    void createBindGroupLayout();
    void createBindGroup();
    void cleanup();
    bool shouldUpdateCells();

public:
    class InitializationError : public std::runtime_error {
        public:
            InitializationError(const std::string& msg) 
                : std::runtime_error("Initialization failed: " + msg) {}
    };
    class RuntimeError : public std::runtime_error {
        public:
            RuntimeError(const std::string& msg) 
                : std::runtime_error("Encountered an unexpected runtime error: " + msg) {}
    };
    Life();
    ~Life();

    const wgpu::Instance& getInstance() const { return instance; }
    const wgpu::Adapter& getAdapter() const { return adapter; }
    const wgpu::Device& getDevice() const { return device; }
    const wgpu::Queue& getQueue() const { return queue; }
    const wgpu::Surface& getSurface() const { return surface; }
    const wgpu::SurfaceConfiguration& getSurfaceConfig() const { return surfaceConfig; }
    const wgpu::RenderPipeline& getRenderPipeline() const { return renderPipeline; }
    const wgpu::ComputePipeline& getSimulationPipeline() const { return simulationPipeline; }
    const wgpu::Buffer& getVertexBuffer() const { return vertexBuffer; }
    const wgpu::Buffer& getUniformBuffer() const { return uniformBuffer; }
    const wgpu::BindGroupLayout& getBindGroupLayout() const { return bindGroupLayout; }
    const wgpu::BindGroup& getBindGroup() const { return bindGroup; }
    void renderFrame();
    void handleResize();

};


