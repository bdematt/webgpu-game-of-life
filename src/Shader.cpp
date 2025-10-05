#include "Shader.h"
#include <fstream>
#include <sstream>
#include <iostream>

std::string Shader::loadShaderCode(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open shader file: " << filepath << std::endl;
        return "";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

wgpu::ShaderModule Shader::loadModuleFromFile(wgpu::Device device, const std::string& filepath) {
    std::string code = loadShaderCode(filepath);
    return createFromCode(device, code);
}

wgpu::ShaderModule Shader::createFromCode(wgpu::Device device, const std::string& wgslCode) {
    wgpu::ShaderModuleWGSLDescriptor wgslDesc {};
    wgslDesc.setDefault();
    wgslDesc.code = wgslCode.c_str();

    wgpu::ShaderModuleDescriptor shaderDesc {};
    shaderDesc.setDefault();
    shaderDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&wgslDesc);
    
    return device.createShaderModule(shaderDesc);
}