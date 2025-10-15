#include <metal_stdlib>
using namespace metal;

constant float GRAVITY = -15.0f;
constant uint MAX_INSTANCES = 16384; // tune to your MAX_INSTANCES

struct VerletObject {
    float3 current;
    float3 previous;
    float3 acceleration;
    float radius;
    // pad to multiple of 16 bytes if necessary
};

kernel void kernel_applyForces(device VerletObject *objects [[buffer(0)]],
                               constant uint &numActive [[buffer(1)]],
                               uint gid [[thread_position_in_grid]])
{
    if (gid >= numActive) return;
    objects[gid].acceleration.y += GRAVITY;
}

kernel void kernel_updatePositions(device VerletObject *objects [[buffer(0)]],
                                   constant uint &numActive [[buffer(1)]],
                                   constant float &dt [[buffer(2)]],
                                   uint gid [[thread_position_in_grid]])
{
    if (gid >= numActive) return;
    VerletObject obj = objects[gid];
    float3 disp = obj.current - obj.previous;
    objects[gid].previous = obj.current;
    float3 accel = obj.acceleration * (dt * dt);
    objects[gid].current = obj.current + disp + accel;
    objects[gid].acceleration = float3(0.0, 0.0, 0.0);
}

kernel void kernel_applyConstraintsBox(device VerletObject *objects [[buffer(0)]],
                                       constant uint &numActive [[buffer(1)]],
                                       constant float3 &containerPosition [[buffer(2)]],
                                       constant float &bWidth [[buffer(3)]],
                                       uint gid [[thread_position_in_grid]])
{
    if (gid >= numActive) return;
    device VerletObject &obj = objects[gid];
    float3 minB = containerPosition - float3(bWidth, bWidth, bWidth);
    float3 maxB = containerPosition + float3(bWidth, bWidth, bWidth);

    if (obj.current.x < minB.x) {
        float disp = obj.current.x - obj.previous.x;
        obj.current.x = minB.x;
        obj.previous.x = obj.current.x + disp;
    }
    if (obj.current.x > maxB.x) {
        float disp = obj.current.x - obj.previous.x;
        obj.current.x = maxB.x;
        obj.previous.x = obj.current.x + disp;
    }
    if (obj.current.y < minB.y) {
        float disp = obj.current.y - obj.previous.y;
        obj.current.y = minB.y;
        obj.previous.y = obj.current.y + disp;
    }
    if (obj.current.y > maxB.y) {
        float disp = obj.current.y - obj.previous.y;
        obj.current.y = maxB.y;
        obj.previous.y = obj.current.y + disp;
    }
    if (obj.current.z < minB.z) {
        float disp = obj.current.z - obj.previous.z;
        obj.current.z = minB.z;
        obj.previous.z = obj.current.z + disp;
    }
    if (obj.current.z > maxB.z) {
        float disp = obj.current.z - obj.previous.z;
        obj.current.z = maxB.z;
        obj.previous.z = obj.current.z + disp;
    }
    objects[gid] = obj;
}

/*
 Simple pairwise collision: each thread considers collisions between objects[gid] and every other object.
 Note: this is simple and demonstrates parallelism. For large N you should implement a spatial hash/grid
 on GPU (more complex).
*/
kernel void kernel_pairwiseCollisions(device VerletObject *objects [[buffer(0)]],
                                      constant uint &numActive [[buffer(1)]],
                                      uint gid [[thread_position_in_grid]])
{
    if (gid >= numActive) return;
    VerletObject a = objects[gid];
    for (uint j = 0; j < numActive; ++j) {
        if (j == gid) continue;
        VerletObject b = objects[j];
        float3 axis = a.current - b.current;
        float dist = length(axis);
        float minDist = a.radius + b.radius;
        if (dist > 0.00001 && dist < minDist) {
            float3 norm = axis / dist;
            float delta = minDist - dist;
            // move current object half the penetration out - note: this ignores mass and may cause jitter
            a.current += 0.5f * delta * norm;
            // Note: we don't move b here to avoid race conditions; a more complete approach needs read-modify-write with atomics or a two-pass solver.
        }
    }
    // write back a
    objects[gid] = a;
}
