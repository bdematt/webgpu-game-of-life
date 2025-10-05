// ======================================================
// Bindings
// ======================================================
// The grid dimensions ex. [256, 256] for 256x256 size grid (Life::GRID_DIMENSIONS)
@group(0) @binding(0) var<uniform> grid: vec2f; 

// Cell state buffers (Alternative between Life::PingPongBuffers::read and ::write each frame)
// Stored as u32 (not bool) for arithmetic convenience and storage buffer compatibility
// This could be optimized with bitpacking and bitwise operations, but memory savings are negligible at low grid sizes
@group(0) @binding(1) var<storage> cellStateIn: array<u32>; // Current state
@group(0) @binding(2) var<storage, read_write> cellStateOut: array<u32>; // Next state

// ======================================================
// Vertex Shader Input/Output Structs
// ======================================================
struct VertexInput {
  @location(0) pos: vec2f, // Vertex (buffered from Life::vertexBuffer)
  @builtin(instance_index) instance: u32, // Instance index (one for each vertex in each cell)
};
struct VertexOutput {
  @builtin(position) pos: vec4f, // Clip space position, must be returned to GPU
  @location(0) cell: vec2f, // Cell position in grid, use in fragment shader
};

// ======================================================
// Vertex Shader
// ======================================================
@vertex
fn vertexMain(input: VertexInput) -> VertexOutput  {
  // Convert instance index to cell position
  let i = f32(input.instance); // Get instance index as float
  let cell = vec2f(i % grid.x, floor(i / grid.x)); // Convert to cell coordinates (x,y)

  // Get cell state (0 or 1)
  let state = f32(cellStateIn[input.instance]);

  // Convert cell's grid position to clip space
  let cellOffset = cell / grid * 2;
  let gridPos = (input.pos * state + 1) / grid - 1 + cellOffset;

  // Return output to GPU
  var output: VertexOutput;
  output.pos = vec4f(gridPos, 0, 1);
  output.cell = cell;
  return output;
}

// ======================================================
// Fragment Shader
// ======================================================
@fragment
// Takes VertexOutput (see above) as fragment input
// Runs for each pixel in each fragment in each cell
fn fragmentMain(input: VertexOutput) -> @location(0) vec4f {
  // Color based on cell position in grid (gradient effect calculated from x, y position)
  let c = input.cell / grid;
  return vec4f(c.x, c.y, 1-c.x, 1);
}

// ======================================================
// Compute Shader Helper Functions
// ======================================================
fn cellIndex(cell: vec2u) -> u32 {
  // Cells are stored in a 1D array, so converts 2D (x,y) to 1D index
  // Wraps around edges using modulo operator, so opposite edges are connected
  return (cell.y % u32(grid.y)) * u32(grid.x) +
         (cell.x % u32(grid.x));
}

fn cellActive(x: u32, y: u32) -> u32 {
  // Gets cell active state (0 or 1) for index converted from (x,y)
  return cellStateIn[cellIndex(vec2(x, y))];
}

// ======================================================
// Compute Shader
// ======================================================

// Default to 8, but dynamically overriden in compute pipeline
override WORKGROUP_SIZE: u32 = 8;

@compute
@workgroup_size(WORKGROUP_SIZE, WORKGROUP_SIZE)
fn computeMain(@builtin(global_invocation_id) cell: vec3u) {
  // Count active neighbors
  let activeNeighbors = cellActive(cell.x+1, cell.y+1) +
                        cellActive(cell.x+1, cell.y) +
                        cellActive(cell.x+1, cell.y-1) +
                        cellActive(cell.x, cell.y-1) +
                        cellActive(cell.x-1, cell.y-1) +
                        cellActive(cell.x-1, cell.y) +
                        cellActive(cell.x-1, cell.y+1) +
                        cellActive(cell.x, cell.y+1);
  // Apply Conway's Game of Life rules
  let i = cellIndex(cell.xy);
  switch activeNeighbors {
    case 2: { // Active cells with 2 neighbors stay active.
      cellStateOut[i] = cellStateIn[i];
    }
    case 3: { // Cells with 3 neighbors become or stay active.
      cellStateOut[i] = 1;
    }
    default: { // Cells with < 2 or > 3 neighbors become inactive.
      cellStateOut[i] = 0;
    }
  }
}