# Verlet Integration - Parallel Computing Demonstration

A comprehensive demonstration of parallel computing techniques applied to real-time physics simulation using Verlet integration.

## Project Overview

This project showcases different parallelization strategies for physics simulation:
- **Sequential**: Baseline single-threaded implementation
- **OpenMP**: Automatic multi-threading with compiler directives
- **Pthread**: Manual thread management and work distribution
- **GPU Compute**: OpenGL compute shader acceleration
- **Hybrid**: Combined CPU-GPU processing

## Features

- Interactive 3D physics simulation with thousands of objects
- Real-time performance metrics (FPS, physics computation time)
- Menu-driven interface to compare different parallelization methods
- Visual feedback with velocity-based coloring
- Configurable number of physics substeps for accuracy

## Requirements

### Software Dependencies
- GCC with OpenMP support (gcc 4.9+)
- OpenGL 4.3+
- GLEW (OpenGL Extension Wrangler)
- GLFW3 (Window management)
- pthread library (usually included)

### Hardware
- Multi-core CPU (4+ cores recommended)
- OpenGL 4.3 compatible GPU
- 4GB RAM minimum

## Installation

### Ubuntu/Debian
```bash
sudo apt-get update
sudo apt-get install build-essential
sudo apt-get install libglew-dev libglfw3-dev
sudo apt-get install mesa-utils
```

### Fedora/RHEL
```bash
sudo dnf install gcc make
sudo dnf install glew-devel glfw-devel
```

### macOS
```bash
brew install glew glfw
```

## Building

### Quick Build
```bash
make
```

### Specific Build Modes
```bash
make sequential  # Build without OpenMP
make openmp      # Build with OpenMP optimizations
make pthread     # Build with pthread support
make gpu         # Build with GPU compute
make debug       # Build with debug symbols
```

### Clean Build
```bash
make clean
make
```

## Running

```bash
./bin/verlet_parallel
```

### Controls
- **V**: Add objects to simulation
- **G**: Apply gravity pulse
- **Arrow Keys**: Move container
- **W/S**: Zoom camera in/out
- **ESC**: Return to menu

## Menu Options

### 1. Sequential (Baseline)
Single-threaded implementation for performance comparison.
- Best for: Understanding baseline performance
- Expected FPS: 30-60 with 1000-2000 objects

### 2. OpenMP Parallelization
Automatic parallelization using compiler directives.
- Best for: Easy parallel implementation
- Expected FPS: 60+ with 5000-10000 objects
- Scales with: CPU core count

### 3. Pthread Manual Threading
Explicit thread management with worker pools.
- Best for: Fine-grained control over threading
- Expected FPS: 60+ with 5000-10000 objects
- Demonstrates: Thread synchronization, barriers

### 4. GPU Compute Shader
Full physics pipeline on GPU using OpenGL compute shaders.
- Best for: Maximum performance
- Expected FPS: 60+ with 20000+ objects
- Requires: OpenGL 4.3+ compatible GPU

### 5. Hybrid CPU-GPU
Load-balanced processing between CPU and GPU.
- Best for: Demonstrating heterogeneous computing
- Expected FPS: 60+ with 15000+ objects
- Shows: Dynamic load distribution

### 6. Performance Comparison
Runs all methods and generates comparative metrics.
- Outputs: FPS, speedup factors, efficiency metrics
- Generates: comparison.txt with detailed results

## Performance Metrics

The program tracks and displays:
- **FPS (Frames Per Second)**: Overall rendering performance
- **Physics Time**: Time spent in physics computation (ms)
- **Object Count**: Number of active physics objects
- **Speedup**: Parallel time / Sequential time
- **Efficiency**: Speedup / Number of cores

## Code Structure

```
.
├── main.c                      # Main program with menu system
├── parallel_verlet.h           # Parallel implementation header
├── parallel_verlet.c           # Parallel implementations
├── verlet.h/c                  # Core physics engine
├── graphics.h/c                # OpenGL rendering
├── shader.h/c                  # Shader management
├── model.h/c                   # 3D model loading
├── camera.h/c                  # Camera system
├── peripheral.h/c              # Input handling
├── shaders/
│   ├── verlet_compute.glsl    # GPU compute shader
│   ├── instance_vertex.glsl   # Instanced rendering
│   └── instance_fragment.glsl # Fragment shader
├── models/
│   ├── sphere.obj             # Sphere mesh
│   └── cube.obj               # Container mesh
├── Makefile                   # Build configuration
└── README.md                  # This file
```

## Parallelization Details

### OpenMP Implementation
```c
#pragma omp parallel for schedule(static)
for (int i = 0; i < count; i++) {
    // Physics computation per object
}
```
- **Advantages**: Simple, automatic load balancing
- **Disadvantages**: Less control over thread behavior

### Pthread Implementation
- Worker thread pool with explicit work distribution
- Uses pthread_barrier for synchronization
- Demonstrates manual thread management

### GPU Compute Shader
- Processes 256 objects per work group
- All data stays in GPU memory
- Eliminates CPU-GPU transfer bottleneck

### Hybrid Approach
- 25% of objects on CPU (OpenMP)
- 75% of objects on GPU (Compute shader)
- Demonstrates heterogeneous computing

## Performance Analysis

### Amdahl's Law
The speedup is limited by the sequential portion:
```
Speedup = 1 / (S + P/N)
Where:
  S = Sequential portion (5-10%)
  P = Parallel portion (90-95%)
  N = Number of processors
```

### Expected Speedup (8-core CPU)
- **OpenMP**: 5-7x speedup
- **Pthread**: 5-7x speedup
- **GPU**: 10-50x speedup (depends on GPU)
- **Hybrid**: 8-20x speedup

## Troubleshooting

### Low FPS in all modes
- Reduce NUM_SUBSTEPS in main.c
- Check GPU drivers are up to date
- Verify OpenGL 4.3 support: `glxinfo | grep "OpenGL version"`

### Compute shader not working
- Check GPU supports OpenGL 4.3+
- Verify GLSL compute shader compilation errors in console
- Try updating GPU drivers

### Compilation errors
- Ensure all dependencies are installed
- Check GCC version: `gcc --version` (need 4.9+)
- Verify OpenMP support: `echo |cpp -fopenmp -dM |grep -i open`

## Future Enhancements

- CUDA implementation for NVIDIA GPUs
- OpenCL for cross-platform GPU acceleration
- Advanced spatial partitioning (octree, BVH)
- SIMD optimizations (AVX, SSE)
- Distributed computing with MPI
- Performance profiling integration

## Educational Value

This project demonstrates:
1. **Amdahl's Law**: Understanding speedup limitations
2. **Load Balancing**: Distributing work evenly
3. **Race Conditions**: Thread-safe collision detection
4. **Memory Coalescing**: GPU memory access patterns
5. **Heterogeneous Computing**: CPU+GPU collaboration
6. **Performance Measurement**: Proper benchmarking techniques

## References

- [Verlet Integration](https://en.wikipedia.org/wiki/Verlet_integration)
- [OpenMP Specifications](https://www.openmp.org/)
- [OpenGL Compute Shaders](https://www.khronos.org/opengl/wiki/Compute_Shader)
- [Pthread Tutorial](https://computing.llnl.gov/tutorials/pthreads/)

## License

This project is for educational purposes. Feel free to use and modify.

## Author

Parallel Computing Demonstration Project
Contact: [Your details here]

## Acknowledgments

- Original Verlet integration implementation
- OpenGL and GLFW communities
- Parallel computing research community