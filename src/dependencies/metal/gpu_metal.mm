// gpu_metal.mm
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "verlet.h" // must match the C struct layout exactly

static id<MTLDevice> device = nil;
static id<MTLCommandQueue> queue = nil;
static id<MTLLibrary> library = nil;
static id<MTLComputePipelineState> pipe_forces = nil;
static id<MTLComputePipelineState> pipe_update = nil;
static id<MTLComputePipelineState> pipe_constraints = nil;
static id<MTLComputePipelineState> pipe_collisions = nil;
static id<MTLBuffer> buffer_objects = nil;
static uint maxInstances = 16384;
static size_t verletObjectSize = 0;

// helper: round up
static NSUInteger roundUp(NSUInteger value, NSUInteger multiple) {
    return ((value + multiple - 1) / multiple) * multiple;
}

extern "C" void setupGPUCompute()
{
    @autoreleasepool {
        device = MTLCreateSystemDefaultDevice();
        if (!device) {
            printf("Metal device unavailable on this machine\n");
            return;
        }
        queue = [device newCommandQueue];

        NSError *err = nil;
        NSString *path = @"verlet_kernels.metal";
        NSString *src = [NSString stringWithContentsOfFile:path encoding:NSUTF8StringEncoding error:&err];
        if (err || src == nil) {
            NSLog(@"Failed to read metal shader: %@", err);
            return;
        }
        library = [device newLibraryWithSource:src options:nil error:&err];
        if (err || library == nil) {
            NSLog(@"Failed to compile metal library: %@", err);
            return;
        }

        id<MTLFunction> f_forces = [library newFunctionWithName:@"kernel_applyForces"];
        id<MTLFunction> f_update = [library newFunctionWithName:@"kernel_updatePositions"];
        id<MTLFunction> f_constraints = [library newFunctionWithName:@"kernel_applyConstraintsBox"];
        id<MTLFunction> f_collide = [library newFunctionWithName:@"kernel_pairwiseCollisions"];

        pipe_forces = [device newComputePipelineStateWithFunction:f_forces error:&err];
        pipe_update = [device newComputePipelineStateWithFunction:f_update error:&err];
        pipe_constraints = [device newComputePipelineStateWithFunction:f_constraints error:&err];
        pipe_collisions = [device newComputePipelineStateWithFunction:f_collide error:&err];

        // allocate buffer for VerletObject array
        verletObjectSize = sizeof(VerletObject); // from verlet.h
        buffer_objects = [device newBufferWithLength:verletObjectSize * maxInstances options:MTLResourceStorageModeShared];
        memset([buffer_objects contents], 0, [buffer_objects length]);

        printf("Metal compute setup complete. maxInstances=%u object_size=%zu\n", maxInstances, verletObjectSize);
    }
}

extern "C" void cleanupGPUCompute()
{
    @autoreleasepool {
        buffer_objects = nil;
        pipe_forces = nil;
        pipe_update = nil;
        pipe_constraints = nil;
        pipe_collisions = nil;
        library = nil;
        queue = nil;
        device = nil;
    }
}

// helper: copy host VerletObject array into GPU buffer
static void copyToGPU(VerletObject *verlets, int numActive) {
    if (!buffer_objects) return;
    size_t copyBytes = verletObjectSize * numActive;
    memcpy([buffer_objects contents], verlets, copyBytes);
}

// helper: copy back GPU -> host
static void copyFromGPU(VerletObject *verlets, int numActive) {
    if (!buffer_objects) return;
    size_t copyBytes = verletObjectSize * numActive;
    memcpy(verlets, [buffer_objects contents], copyBytes);
}

extern "C" void applyForcesGPU(VerletObject *verlets, int numActive)
{
    @autoreleasepool {
        if (!device) return;
        copyToGPU(verlets, numActive);

        id<MTLCommandBuffer> cmd = [queue commandBuffer];
        id<MTLComputeCommandEncoder> enc = [cmd computeCommandEncoder];
        [enc setComputePipelineState:pipe_forces];
        [enc setBuffer:buffer_objects offset:0 atIndex:0];

        uint n = (uint)numActive;
        [enc setBytes:&n length:sizeof(uint) atIndex:1];

        MTLSize grid = MTLSizeMake(numActive, 1, 1);
        NSUInteger w = pipe_forces.maxTotalThreadsPerThreadgroup;
        if (w == 0) w = 64;
        MTLSize tg = MTLSizeMake((w > numActive) ? numActive : w, 1, 1);
        [enc dispatchThreads:grid threadsPerThreadgroup:tg];
        [enc endEncoding];
        [cmd commit];
        [cmd waitUntilCompleted];

        copyFromGPU(verlets, numActive);
    }
}

extern "C" void updatePositionsGPU(VerletObject *verlets, int numActive, float dt)
{
    @autoreleasepool {
        if (!device) return;
        copyToGPU(verlets, numActive);

        id<MTLCommandBuffer> cmd = [queue commandBuffer];
        id<MTLComputeCommandEncoder> enc = [cmd computeCommandEncoder];
        [enc setComputePipelineState:pipe_update];
        [enc setBuffer:buffer_objects offset:0 atIndex:0];

        uint n = (uint)numActive;
        [enc setBytes:&n length:sizeof(uint) atIndex:1];
        [enc setBytes:&dt length:sizeof(float) atIndex:2];

        MTLSize grid = MTLSizeMake(numActive, 1, 1);
        NSUInteger w = pipe_update.maxTotalThreadsPerThreadgroup;
        if (w == 0) w = 64;
        MTLSize tg = MTLSizeMake((w > numActive) ? numActive : w, 1, 1);

        [enc dispatchThreads:grid threadsPerThreadgroup:tg];
        [enc endEncoding];
        [cmd commit];
        [cmd waitUntilCompleted];

        copyFromGPU(verlets, numActive);
    }
}

extern "C" void applyConstraintsGPU(VerletObject *verlets, int numActive, mfloat_t *containerPos)
{
    @autoreleasepool {
        if (!device) return;
        copyToGPU(verlets, numActive);

        id<MTLCommandBuffer> cmd = [queue commandBuffer];
        id<MTLComputeCommandEncoder> enc = [cmd computeCommandEncoder];
        [enc setComputePipelineState:pipe_constraints];
        [enc setBuffer:buffer_objects offset:0 atIndex:0];

        uint n = (uint)numActive;
        [enc setBytes:&n length:sizeof(uint) atIndex:1];

        // containerPosition -> float3
        float3 center = { containerPos[0], containerPos[1], containerPos[2] };
        [enc setBytes:&center length:sizeof(center) atIndex:2];

        float bWidth = (float)CONTAINER_RADIUS;
        [enc setBytes:&bWidth length:sizeof(float) atIndex:3];

        MTLSize grid = MTLSizeMake(numActive, 1, 1);
        NSUInteger w = pipe_constraints.maxTotalThreadsPerThreadgroup;
        if (w == 0) w = 64;
        MTLSize tg = MTLSizeMake((w > numActive) ? numActive : w, 1, 1);

        [enc dispatchThreads:grid threadsPerThreadgroup:tg];
        [enc endEncoding];
        [cmd commit];
        [cmd waitUntilCompleted];

        copyFromGPU(verlets, numActive);
    }
}

extern "C" void applyGridCollisionsGPU(VerletObject *verlets, int numActive)
{
    @autoreleasepool {
        if (!device) return;
        copyToGPU(verlets, numActive);

        id<MTLCommandBuffer> cmd = [queue commandBuffer];
        id<MTLComputeCommandEncoder> enc = [cmd computeCommandEncoder];
        [enc setComputePipelineState:pipe_collisions];
        [enc setBuffer:buffer_objects offset:0 atIndex:0];

        uint n = (uint)numActive;
        [enc setBytes:&n length:sizeof(uint) atIndex:1];

        MTLSize grid = MTLSizeMake(numActive, 1, 1);
        NSUInteger w = pipe_collisions.maxTotalThreadsPerThreadgroup;
        if (w == 0) w = 64;
        MTLSize tg = MTLSizeMake((w > numActive) ? numActive : w, 1, 1);

        [enc dispatchThreads:grid threadsPerThreadgroup:tg];
        [enc endEncoding];
        [cmd commit];
        [cmd waitUntilCompleted];

        copyFromGPU(verlets, numActive);
    }
}
