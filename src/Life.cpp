#include "Life.h"
#include "webgpu.hpp"
#include "Shader.h"
#include <random>
#include <emscripten/html5.h>

Life::Life()
    : cellStateArray(GRID_SIZE * GRID_SIZE)
    , lastFrameTime(std::chrono::steady_clock::now())
{
    requestAdapter();
    requestDevice();
    createSurface();
    configureSurface();
    createBindGroupLayout();
    createPipelines();
    createVertexBuffer();
    createStorageBuffers();
    createUniformBuffer();
    createBindGroup();
}

Life::~Life()
{
    cleanup();
}

void Life::requestAdapter()
{
    wgpu::RequestAdapterOptions adapterOptions {};
    adapterOptions.setDefault();
    adapter = instance.requestAdapter(adapterOptions);
    if (!adapter) throw Life::InitializationError("Failed to request adapter");
}

void Life::requestDevice()
{
    wgpu::DeviceDescriptor deviceDesc {};
    deviceDesc.setDefault();
    device = adapter.requestDevice(deviceDesc);
    if (!device) throw Life::InitializationError("Failed to request device");
    queue = device.getQueue();
    if (!queue) throw Life::InitializationError("Failed to get queue");

}

void Life::createSurface()
{
    wgpu::SurfaceDescriptorFromCanvasHTMLSelector surfaceSelector {};
    surfaceSelector.setDefault();
    surfaceSelector.selector = "#canvas";

    wgpu::SurfaceDescriptor surfaceDesc {};
    surfaceDesc.setDefault();
    surfaceDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&surfaceSelector);
    surface = instance.createSurface(surfaceDesc);

    if (!surface) throw Life::InitializationError("Failed to create surface");    
}

void Life::configureSurface()
{
    surfaceConfig.setDefault();
    surfaceConfig.device = device;
    surfaceConfig.format = getSurface().getPreferredFormat(adapter);
    surfaceConfig.usage = wgpu::TextureUsage::RenderAttachment;
    int width, height;
    emscripten_get_canvas_element_size("#canvas", &width, &height);
    surfaceConfig.width = width;
    surfaceConfig.height = height;
    surface.configure(surfaceConfig);
}

void Life::createBindGroupLayout()
{
    std::array<wgpu::BindGroupLayoutEntry, 3> entries;

    // Binding 0: Grid uniform buffer
    wgpu::BindGroupLayoutEntry uniformBindGroupLayoutEntry {};
    uniformBindGroupLayoutEntry.setDefault();
    uniformBindGroupLayoutEntry.binding = 0;
    uniformBindGroupLayoutEntry.visibility = wgpu::ShaderStage::Vertex | 
                                             wgpu::ShaderStage::Fragment | 
                                             wgpu::ShaderStage::Compute;
    uniformBindGroupLayoutEntry.buffer.type = wgpu::BufferBindingType::Uniform;
    uniformBindGroupLayoutEntry.buffer.minBindingSize = sizeof(GRID_DIMENSIONS);
    entries[0] = uniformBindGroupLayoutEntry;

    // Binding 1: Cell state INPUT buffer (read-only storage)
    wgpu::BindGroupLayoutEntry inputStorageBindGroupLayoutEntry {};
    inputStorageBindGroupLayoutEntry.setDefault();
    inputStorageBindGroupLayoutEntry.binding = 1;
    inputStorageBindGroupLayoutEntry.visibility = wgpu::ShaderStage::Vertex | 
                                                   wgpu::ShaderStage::Fragment | 
                                                   wgpu::ShaderStage::Compute;
    inputStorageBindGroupLayoutEntry.buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
    inputStorageBindGroupLayoutEntry.buffer.minBindingSize = cellStateArray.size() * sizeof(uint32_t);
    entries[1] = inputStorageBindGroupLayoutEntry;

    // Binding 2: Cell state OUTPUT buffer (read-write storage)
    wgpu::BindGroupLayoutEntry outputStorageBindGroupLayoutEntry {};
    outputStorageBindGroupLayoutEntry.setDefault();
    outputStorageBindGroupLayoutEntry.binding = 2;
    outputStorageBindGroupLayoutEntry.visibility = wgpu::ShaderStage::Compute;
    outputStorageBindGroupLayoutEntry.buffer.type = wgpu::BufferBindingType::Storage;
    outputStorageBindGroupLayoutEntry.buffer.minBindingSize = cellStateArray.size() * sizeof(uint32_t);
    entries[2] = outputStorageBindGroupLayoutEntry;

    wgpu::BindGroupLayoutDescriptor bindGroupLayoutDesc {};
    bindGroupLayoutDesc.setDefault();
    bindGroupLayoutDesc.label = "Cell bind group layout";
    bindGroupLayoutDesc.entryCount = 3;
    bindGroupLayoutDesc.entries = entries.data();

    bindGroupLayout = getDevice().createBindGroupLayout(bindGroupLayoutDesc);
    if (!bindGroupLayout) throw Life::InitializationError("Failed to create bind group layout");   
}

void Life::createPipelines()
{
    wgpu::ShaderModule cellShaderModule = Shader::loadModuleFromFile(
        getDevice(),
        "/shaders/shader.wgsl"
    );

    wgpu::PipelineLayoutDescriptor layoutDesc {};
    layoutDesc.setDefault();
    layoutDesc.bindGroupLayoutCount = 1;
    layoutDesc.bindGroupLayouts = reinterpret_cast<const WGPUBindGroupLayout*>(&getBindGroupLayout());
    wgpu::PipelineLayout pipelineLayout = getDevice().createPipelineLayout(layoutDesc);

    // Vertex setup
    wgpu::VertexAttribute vertexAttribute {};
    vertexAttribute.setDefault();
    vertexAttribute.format = wgpu::VertexFormat::Float32x2;
    vertexAttribute.offset = 0;
    vertexAttribute.shaderLocation = 0;

    wgpu::VertexBufferLayout vertexBufferLayout {};
    vertexBufferLayout.setDefault();
    vertexBufferLayout.stepMode = wgpu::VertexStepMode::Vertex;
    vertexBufferLayout.arrayStride = 8;
    vertexBufferLayout.attributeCount = 1;
    vertexBufferLayout.attributes = &vertexAttribute;

    // Pipeline descriptor
    wgpu::RenderPipelineDescriptor pipelineDesc {};
    pipelineDesc.setDefault();
    pipelineDesc.label = "Cell pipeline";
    pipelineDesc.layout = pipelineLayout;

    pipelineDesc.vertex.module = cellShaderModule;
    pipelineDesc.vertex.entryPoint = "vertexMain";
    pipelineDesc.vertex.bufferCount = 1;
    pipelineDesc.vertex.buffers = &vertexBufferLayout;

    wgpu::ColorTargetState colorTarget {};
    colorTarget.setDefault();
    colorTarget.format = surfaceConfig.format;
    colorTarget.writeMask = wgpu::ColorWriteMask::All;

    wgpu::FragmentState fragmentState {};
    fragmentState.setDefault();
    fragmentState.module = cellShaderModule;
    fragmentState.entryPoint = "fragmentMain";
    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;

    pipelineDesc.fragment = &fragmentState;

    renderPipeline = getDevice().createRenderPipeline(pipelineDesc);

    // Create compute pipeline
    wgpu::PipelineLayoutDescriptor computeLayoutDesc {};
    computeLayoutDesc.setDefault();
    computeLayoutDesc.bindGroupLayoutCount = 1;
    computeLayoutDesc.bindGroupLayouts = reinterpret_cast<const WGPUBindGroupLayout*>(&getBindGroupLayout());
    wgpu::PipelineLayout computePipelineLayout = getDevice().createPipelineLayout(computeLayoutDesc);

    // Define the override constant
    wgpu::ConstantEntry constantEntry {};
    constantEntry.key = "WORKGROUP_SIZE";
    constantEntry.value = static_cast<double>(WORKGROUP_SIZE);

    wgpu::ComputePipelineDescriptor computePipelineDesc {};
    computePipelineDesc.setDefault();
    computePipelineDesc.label = "Simulation pipeline";
    computePipelineDesc.layout = computePipelineLayout;
    computePipelineDesc.compute.module = cellShaderModule;
    computePipelineDesc.compute.entryPoint = "computeMain";
    computePipelineDesc.compute.constantCount = 1;
    computePipelineDesc.compute.constants = &constantEntry;

    simulationPipeline = getDevice().createComputePipeline(computePipelineDesc);
    if (!simulationPipeline) throw Life::InitializationError("Failed to create compute pipeline");

    // Clean up temporary resources
    computePipelineLayout.release();
    cellShaderModule.release();
}

void Life::createVertexBuffer()
{
    wgpu::BufferDescriptor bufferDesc {};
    bufferDesc.setDefault();
    bufferDesc.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
    bufferDesc.size = sizeof(VERTICES);
    
    vertexBuffer = getDevice().createBuffer(bufferDesc);
    if (!vertexBuffer) throw Life::InitializationError("Failed to create vertexBuffer buffer");

    constexpr uint64_t BUFFER_OFFSET = 0;
    getQueue().writeBuffer(vertexBuffer, BUFFER_OFFSET, VERTICES, sizeof(VERTICES));
}

void Life::createUniformBuffer()
{
    wgpu::BufferDescriptor bufferDesc {};
    bufferDesc.setDefault();
    bufferDesc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
    bufferDesc.size = sizeof(GRID_DIMENSIONS);
    
    uniformBuffer = getDevice().createBuffer(bufferDesc);
    if (!uniformBuffer) throw Life::InitializationError("Failed to create uniform buffer");

    constexpr uint64_t BUFFER_OFFSET = 0;
    getQueue().writeBuffer(uniformBuffer, BUFFER_OFFSET, GRID_DIMENSIONS, sizeof(GRID_DIMENSIONS));
}

void Life::createStorageBuffers()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 1);
    for (auto& cell : cellStateArray) {
        cell = dis(gen);
    }
    
    wgpu::BufferDescriptor bufferDesc {};
    bufferDesc.label = "Cell State Storage";
    bufferDesc.size = cellStateArray.size() * sizeof(uint32_t);
    bufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst; 
    
    // Create read buffer
    cellBuffers.read = device.createBuffer(bufferDesc);
    if (!cellBuffers.read) throw Life::InitializationError("Failed to create read storage buffer");
    
    // Create write buffer
    cellBuffers.write = device.createBuffer(bufferDesc);
    if (!cellBuffers.write) throw Life::InitializationError("Failed to create write storage buffer");
    
    // Initialize both buffers with the same data
    constexpr uint64_t BUFFER_OFFSET = 0;
    queue.writeBuffer(cellBuffers.read, BUFFER_OFFSET, cellStateArray.data(), 
                     cellStateArray.size() * sizeof(uint32_t));
    queue.writeBuffer(cellBuffers.write, BUFFER_OFFSET, cellStateArray.data(), 
                     cellStateArray.size() * sizeof(uint32_t));
}

void Life::createBindGroup()
{
    // Create bind group A (reads from cellBuffers.read, writes to cellBuffers.write)
    std::array<wgpu::BindGroupEntry, 3> readEntries;

    readEntries[0].setDefault();
    readEntries[0].binding = 0;
    readEntries[0].buffer = getUniformBuffer();
    readEntries[0].offset = 0;
    readEntries[0].size = sizeof(GRID_DIMENSIONS);

    readEntries[1].setDefault();
    readEntries[1].binding = 1;
    readEntries[1].buffer = cellBuffers.read;  // INPUT buffer
    readEntries[1].offset = 0;
    readEntries[1].size = cellStateArray.size() * sizeof(uint32_t);

    // Binding 2 - OUTPUT buffer
    readEntries[2].setDefault();
    readEntries[2].binding = 2;
    readEntries[2].buffer = cellBuffers.write;  // OUTPUT buffer
    readEntries[2].offset = 0;
    readEntries[2].size = cellStateArray.size() * sizeof(uint32_t);

    wgpu::BindGroupDescriptor readBindGroupDesc {};
    readBindGroupDesc.setDefault();
    readBindGroupDesc.label = "Cell renderer bind group A";
    readBindGroupDesc.layout = bindGroupLayout;
    readBindGroupDesc.entryCount = readEntries.size();
    readBindGroupDesc.entries = readEntries.data();

    cellBuffers.readBindGroup = device.createBindGroup(readBindGroupDesc);
    if (!cellBuffers.readBindGroup) throw Life::InitializationError("Failed to create read bindGroup");

    // Create bind group B (reads from cellBuffers.write, writes to cellBuffers.read)
    std::array<wgpu::BindGroupEntry, 3> writeEntries;

    writeEntries[0].setDefault();
    writeEntries[0].binding = 0;
    writeEntries[0].buffer = getUniformBuffer();
    writeEntries[0].offset = 0;
    writeEntries[0].size = sizeof(GRID_DIMENSIONS);

    writeEntries[1].setDefault();
    writeEntries[1].binding = 1;
    writeEntries[1].buffer = cellBuffers.write;
    writeEntries[1].offset = 0;
    writeEntries[1].size = cellStateArray.size() * sizeof(uint32_t);

    writeEntries[2].setDefault();
    writeEntries[2].binding = 2;
    writeEntries[2].buffer = cellBuffers.read;
    writeEntries[2].offset = 0;
    writeEntries[2].size = cellStateArray.size() * sizeof(uint32_t);

    wgpu::BindGroupDescriptor writeBindGroupDesc {};
    writeBindGroupDesc.setDefault();
    writeBindGroupDesc.label = "Cell renderer bind group B";
    writeBindGroupDesc.layout = bindGroupLayout;
    writeBindGroupDesc.entryCount = writeEntries.size();
    writeBindGroupDesc.entries = writeEntries.data();

    cellBuffers.writeBindGroup = device.createBindGroup(writeBindGroupDesc);
    if (!cellBuffers.writeBindGroup) throw Life::InitializationError("Failed to create write bindGroup");
}

void Life::cleanup()
{
    if (bindGroup) bindGroup.release();
    if (cellBuffers.writeBindGroup) cellBuffers.writeBindGroup.release();
    if (cellBuffers.readBindGroup) cellBuffers.readBindGroup.release();
    if (cellBuffers.write) cellBuffers.write.release();
    if (cellBuffers.read) cellBuffers.read.release();
    if (bindGroupLayout) bindGroupLayout.release();
    if (uniformBuffer) uniformBuffer.release();
    if (vertexBuffer) vertexBuffer.release();
    if (renderPipeline) renderPipeline.release();
    if (simulationPipeline) simulationPipeline.release();
    if (surface) surface.release();
    if (queue) queue.release();
    if (device) device.release();
    if (adapter) adapter.release();
    if (instance) instance.release();
}

void Life::renderFrame()
{
    if (!shouldUpdateCells()) {
        return;
    }
    
    // Create command encoder
    wgpu::CommandEncoder encoder = getDevice().createCommandEncoder();

    // Compute Shader Pass
    wgpu::ComputePassEncoder computePass = encoder.beginComputePass();
    computePass.setPipeline(getSimulationPipeline());
    
    // Alternate between bind groups each step
    wgpu::BindGroup currentBindGroup = (step % 2 == 0) 
        ? cellBuffers.readBindGroup 
        : cellBuffers.writeBindGroup;
    computePass.setBindGroup(0, currentBindGroup, 0, nullptr);
    
    // Calculate workgroup count
    const uint32_t workgroupCount = (GRID_SIZE + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE;
    computePass.dispatchWorkgroups(workgroupCount, workgroupCount, 1);
    
    computePass.end();
    
    step++;

    // ========== RENDER PASS - Draw the cells ==========
    wgpu::SurfaceTexture surfaceTexture {};
    getSurface().getCurrentTexture(&surfaceTexture);
    wgpu::Texture texture = surfaceTexture.texture;
    wgpu::TextureView view = texture.createView();

    wgpu::RenderPassColorAttachment colorAttachment {};
    colorAttachment.view = view;
    colorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
    colorAttachment.loadOp = wgpu::LoadOp::Clear;
    colorAttachment.storeOp = wgpu::StoreOp::Store;
    colorAttachment.clearValue = wgpu::Color(0.0, 0.0, 0.4, 1.0);

    wgpu::RenderPassDescriptor renderPassDesc {};
    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &colorAttachment;

    wgpu::RenderPassEncoder renderPass = encoder.beginRenderPass(renderPassDesc);
    renderPass.setPipeline(getRenderPipeline());
    renderPass.setVertexBuffer(0, getVertexBuffer(), 0, sizeof(VERTICES));
    
    // Use the SAME bind group that was just written to by compute pass
    renderPass.setBindGroup(0, currentBindGroup, 0, nullptr);
    
    constexpr uint32_t VERTEX_COUNT = sizeof(VERTICES) / sizeof(float) / 2;
    renderPass.draw(VERTEX_COUNT, GRID_SIZE * GRID_SIZE, 0, 0);
    renderPass.end();

    // Submit all commands
    wgpu::CommandBuffer commandBuffer = encoder.finish();
    getQueue().submit(commandBuffer);
    
    view.release();
}

void Life::handleResize()
{
    int width, height;
    emscripten_get_canvas_element_size("#canvas", &width, &height);

    surfaceConfig.width = static_cast<uint32_t>(width);
    surfaceConfig.height = static_cast<uint32_t>(height);
    surface.configure(surfaceConfig);
}

bool Life::shouldUpdateCells() {
    auto now = std::chrono::steady_clock::now();
    float deltaTime = std::chrono::duration<float>(now - lastFrameTime).count();
    lastFrameTime = now;

    // Cap deltaTime to avoid huge jumps (can happen when browser tab is inactive)
    constexpr float MAX_DELTA_TIME = UPDATE_INTERVAL_SECONDS * 2.0f;
    deltaTime = std::min(deltaTime, MAX_DELTA_TIME);
    
    accumulatedTime += deltaTime;
    
    if (accumulatedTime >= UPDATE_INTERVAL_SECONDS) {
        accumulatedTime -= UPDATE_INTERVAL_SECONDS;
        return true;
    }
    return false;
}