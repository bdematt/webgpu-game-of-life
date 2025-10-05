# Conway's Game of Life
A C++ / WebGPU / Emscripten implementation of [Conway's Game of Life](https://en.wikipedia.org/wiki/Conway%27s_Game_of_Life)

Rendering and cell state computations are done by the GPU using WebGPU

## Demo
[View Live Demo](https://www.google.com)

## Required Tools
1. [Node.js](https://nodejs.org/en/download)
2. [Emscripten SDK (EMSDK)](https://emscripten.org/docs/getting_started/downloads.html)
3. [vcpkg](https://learn.microsoft.com/en-us/vcpkg/get_started/get-started)
4. [CMake](https://cmake.org/download/)
5. [Ninja build system](https://github.com/ninja-build/ninja)

## Setup Instructions

### 1. Install Required Tools
Follow the installation instructions for the [Required Tools](#required-tools) mentioned above.

### 2. Install NPM Dependencies
```bash
# In the project directory
npm install
```
## NPM Scripts

### Development Server
```bash
# Start development server with auto-reload
npm run watch
```

### Manual Serving
```bash
# Just serve the built files (without watching)
npm run serve
```

### Manual Serving
```bash
# Delete dist and build (required if updating shader code, which isn't watched)
npm run clean
```

### Release Build
```bash
# Build for release instead of debug
npm run build:release
```

## Project Structure

```
├── .vscode/
│   ├── c_cpp_properties.json   # VSCode C++ configurations
├── src/                        # C++ source files -- There will be linter errors before building for first time            
│   ├── shaders/  
│   │   ├── shader.wgsl         # Vertex, fragment, and compute shader code
│   ├── index.html              # Emscripten HTML template
│   ├── Life.cpp                # Application data including game state and render pipeline
│   ├── Life.h
│   ├── main.cpp                # Entry point
│   └── Shader.cpp              # Shader (wgsl) loading utility class
│   └── Shader.h
│   └── webgpu.hpp              # Less cumbersome C++ wrapper for C WebGPU API (Credit to https://github.com/eliemichel/LearnWebGPU)
├── build/                      # CMake build artifacts (auto-generated, git ignored)
├── dist/                       # Web output files (auto-generated, git ignored)
├── CMakeLists.txt              # CMake configuration
├── CMakePresets.json           # CMake presets for Emscripten
└── package.json                # Node.js dependencies and scripts
└── vcpkg.json                  # C++ dependencies (auto-installed)
```

## Sources
- [Elie Michel's LearnWebGPU](https://github.com/eliemichel/LearnWebGPU)
- [Google Code Lab's "Your First WebGPU App"](https://codelabs.developers.google.com/your-first-webgpu-app#0)