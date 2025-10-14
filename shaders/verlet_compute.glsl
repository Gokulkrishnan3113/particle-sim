#version 430 core

layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

// Shader Storage Buffer Objects
layout(std430, binding = 0) buffer PositionBuffer {
    vec3 positions[];
};

layout(std430, binding = 1) buffer VelocityBuffer {
    vec3 velocities[];
};

layout(std430, binding = 2) buffer PreviousBuffer {
    vec3 previous[];
};

// Uniforms
uniform float dt;
uniform int numObjects;
uniform vec3 containerPos;
uniform float containerRadius;
uniform float objectRadius;

// Gravity constant
const vec3 GRAVITY = vec3(0.0, -9.81, 0.0);

void main() {
    uint idx = gl_GlobalInvocationID.x;
    
    if (idx >= numObjects) {
        return;
    }
    
    // Current state
    vec3 currentPos = positions[idx];
    vec3 prevPos = previous[idx];
    vec3 acceleration = GRAVITY;
    
    // Verlet integration
    vec3 velocity = currentPos - prevPos;
    vec3 newPos = currentPos + velocity + acceleration * dt * dt;
    
    // Store previous position
    previous[idx] = currentPos;
    
    // Apply container constraint
    vec3 toObj = newPos - containerPos;
    float dist = length(toObj);
    
    if (dist > containerRadius - objectRadius) {
        vec3 n = normalize(toObj);
        newPos = containerPos + n * (containerRadius - objectRadius);
    }
    
    // Simple collision detection with other objects
    // Note: This is a simplified version, proper collision requires spatial partitioning
    for (int i = 0; i < numObjects; i++) {
        if (i == idx) continue;
        
        vec3 diff = newPos - positions[i];
        float d = length(diff);
        float minDist = objectRadius * 2.0;
        
        if (d < minDist && d > 0.0001) {
            vec3 n = normalize(diff);
            float delta = minDist - d;
            newPos += n * delta * 0.5;
        }
    }
    
    // Update position
    positions[idx] = newPos;
    
    // Calculate velocity for visualization
    velocities[idx] = (newPos - prevPos) * 10.0;
}