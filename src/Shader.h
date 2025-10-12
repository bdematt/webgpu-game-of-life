#pragma once
#include <webgpu/webgpu_cpp.h>
#include <string>

class Shader {
public:
    static wgpu::ShaderModule loadModuleFromFile(wgpu::Device device, const std::string& filepath);
    static wgpu::ShaderModule createFromCode(wgpu::Device device, const std::string& wgslCode);
    
private:
    static std::string loadShaderCode(const std::string& filepath);
};