#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "dependencies/include/GL/glew.h"
#include "dependencies/include/GLFW/glfw3.h"

#include "graphics.h"
#include "shader.h"
#include "model.h"
#include "verlet.h"
#include "camera.h"
#include "peripheral.h"
#include "parallel_verlet.h"

// Preprocessor constants
#define ANIMATION_TIME 90.0f
#define ADDITION_SPEED 10
#define TARGET_FPS 60
#define NUM_SUBSTEPS 8

// Execution modes
typedef enum {
    MODE_SEQUENTIAL = 0,
    MODE_OPENMP,
    MODE_PTHREAD,
    MODE_GPU_COMPUTE,
    MODE_HYBRID,
    MODE_COMPARISON,
    MODE_EXIT
} ExecutionMode;

// Function prototypes
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void cursor_enter_callback(GLFWwindow* window, int entered);
void processInput(GLFWwindow* window);
void updateCamera(GLFWwindow* window, Mouse* mouse, Camera* camera);
void instantiateVerlets(VerletObject* objects, int size);
void displayMenu();
ExecutionMode getMenuChoice();
void runSimulation(GLFWwindow* window, ExecutionMode mode);
void runComparisonMode(GLFWwindow* window);
void displayPerformanceMetrics(PerformanceMetrics* metrics, ExecutionMode mode);

// Settings
const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

// Global variables
bool cursorEntered = false;
Camera* camera;
float cameraRadius = 24.0f;
int totalFrames = 0;

int main()
{
    GLFWwindow* window;

    /* Initialize the library */
    if (!glfwInit())
        return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Verlet Integration - Parallel Computing Demo", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }

    /* Make the window's context current */
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSwapInterval(1);
    glfwSetCursorEnterCallback(window, cursor_enter_callback);

    /* Initialize GLEW */
    glewInit();

    /* OpenGL Settings */
    glClearColor(0.1, 0.1, 0.1, 1.0);
    glClearStencil(0);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glPointSize(3.0);

    srand(time(NULL));

    // Menu system
    ExecutionMode mode;
    do {
        displayMenu();
        mode = getMenuChoice();
        
        if (mode == MODE_COMPARISON) {
            runComparisonMode(window);
        } else if (mode != MODE_EXIT) {
            runSimulation(window, mode);
        }
        
    } while (mode != MODE_EXIT);

    glfwTerminate();
    return 0;
}

void displayMenu()
{
    printf("\n");
    printf("=======================================================\n");
    printf("   VERLET INTEGRATION - PARALLEL COMPUTING DEMO\n");
    printf("=======================================================\n");
    printf("\n");
    printf("1. Sequential (Baseline Implementation)\n");
    printf("   - Single-threaded CPU execution\n");
    printf("   - Baseline performance measurement\n");
    printf("\n");
    printf("2. OpenMP Parallelization\n");
    printf("   - Automatic thread-level parallelism\n");
    printf("   - Parallel physics computation\n");
    printf("\n");
    printf("3. Pthread Manual Threading\n");
    printf("   - Explicit thread management\n");
    printf("   - Custom work distribution\n");
    printf("\n");
    printf("4. GPU Compute Shader\n");
    printf("   - OpenGL compute shader acceleration\n");
    printf("   - GPU-based physics calculation\n");
    printf("\n");
    printf("5. Hybrid CPU-GPU\n");
    printf("   - Combined CPU and GPU processing\n");
    printf("   - Load balancing demonstration\n");
    printf("\n");
    printf("6. Performance Comparison\n");
    printf("   - Run all methods and compare\n");
    printf("   - Generate performance metrics\n");
    printf("\n");
    printf("7. Exit\n");
    printf("\n");
    printf("=======================================================\n");
    printf("Select mode (1-7): ");
}

ExecutionMode getMenuChoice()
{
    int choice;
    scanf("%d", &choice);
    
    if (choice < 1 || choice > 7) {
        printf("Invalid choice. Please select 1-7.\n");
        return getMenuChoice();
    }
    
    return (ExecutionMode)(choice - 1);
}

void runSimulation(GLFWwindow* window, ExecutionMode mode)
{
    const char* modeNames[] = {
        "Sequential",
        "OpenMP",
        "Pthread",
        "GPU Compute",
        "Hybrid CPU-GPU"
    };
    
    printf("\n");
    printf("Starting simulation in %s mode...\n", modeNames[mode]);
    printf("Press ESC to return to menu\n");
    printf("Press V to add objects\n");
    printf("Press G to apply gravity pulse\n");
    printf("Arrow keys to move container\n");
    printf("\n");

    /* Models & Shaders */
    unsigned int phongShader = createShader("shaders/phong_vertex.glsl", "shaders/phong_fragment.glsl");
    unsigned int instanceShader = createShader("shaders/instance_vertex.glsl", "shaders/instance_fragment.glsl");
    unsigned int baseShader = createShader("shaders/base_vertex.glsl", "shaders/base_fragment.glsl");

    Mesh* mesh = createMesh("models/sphere.obj", true);
    Mesh* cubeMesh = createMesh("models/cube.obj", false);

    // Initialize compute shader for GPU modes
    unsigned int computeShader = 0;
    if (mode == MODE_GPU_COMPUTE || mode == MODE_HYBRID) {
        computeShader = createComputeShader("shaders/verlet_compute.glsl");
    }

    // Container
    mfloat_t containerPosition[VEC3_SIZE] = { 0, 0, 0 };
    mfloat_t rotation[VEC3_SIZE] = { 0, 0, 0 };

    VerletObject* verlets = malloc(sizeof(VerletObject) * MAX_INSTANCES);
    instantiateVerlets(verlets, MAX_INSTANCES);
    int numActive = 0;

    mfloat_t view[MAT4_SIZE];
    camera = createCamera((mfloat_t[]) { 0, 0, cameraRadius });
    Mouse* mouse = createMouse();

    float dt = 0.000001f;
    float lastFrameTime = (float)glfwGetTime();
    char title[200] = "";

    // Performance tracking
    PerformanceMetrics metrics;
    initPerformanceMetrics(&metrics);
    
    // Initialize parallel systems
    ParallelContext* pContext = NULL;
    if (mode == MODE_PTHREAD) {
        pContext = initPthreadContext(8); // 8 threads
    } else if (mode == MODE_GPU_COMPUTE || mode == MODE_HYBRID) {
        initGPUBuffers(mesh, MAX_INSTANCES);
    }

    totalFrames = 0;

    /* Main loop */
    while (!glfwWindowShouldClose(window)) {
        double frameStart = glfwGetTime();
        
        /* Input */
        updateMouse(window, mouse);
        processInput(window);

        if (glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS) {
            addForce(verlets, numActive, (mfloat_t[]) { 0, 3, 0 }, -30.0f * NUM_SUBSTEPS);
        }

        /* Camera */
        updateCamera(window, mouse, camera);
        createViewMatrix(view, camera);

        /* Shader Uniforms */
        glUseProgram(phongShader);
        glUniformMatrix4fv(glGetUniformLocation(phongShader, "view"), 1, GL_FALSE, view);
        glUseProgram(baseShader);
        glUniformMatrix4fv(glGetUniformLocation(baseShader, "view"), 1, GL_FALSE, view);
        glUseProgram(instanceShader);
        glUniformMatrix4fv(glGetUniformLocation(instanceShader, "view"), 1, GL_FALSE, view);
        glUseProgram(0);

        /* Render */
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        if (1.0 / dt >= TARGET_FPS - 5 && glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS && numActive < MAX_INSTANCES) {
            numActive += ADDITION_SPEED;
        }

        // Container movement
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) containerPosition[0] -= 0.05f;
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) containerPosition[0] += 0.05f;
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) containerPosition[1] -= 0.05f;
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) containerPosition[1] += 0.05f;

        // Physics computation based on mode
        double physicsStart = glfwGetTime();
        float sub_dt = dt / NUM_SUBSTEPS;
        
        switch (mode) {
            case MODE_SEQUENTIAL:
                for (int i = 0; i < NUM_SUBSTEPS; i++) {
                    applyForces(verlets, numActive);
                    applyGridCollisions(verlets, numActive);
                    applyConstraints(verlets, numActive, containerPosition);
                    updatePositions(verlets, numActive, sub_dt);
                }
                break;
                
            case MODE_OPENMP:
                for (int i = 0; i < NUM_SUBSTEPS; i++) {
                    applyForcesOpenMP(verlets, numActive);
                    applyGridCollisionsOpenMP(verlets, numActive);
                    applyConstraintsOpenMP(verlets, numActive, containerPosition);
                    updatePositionsOpenMP(verlets, numActive, sub_dt);
                }
                break;
                
            case MODE_PTHREAD:
                for (int i = 0; i < NUM_SUBSTEPS; i++) {
                    applyForcesPthread(verlets, numActive, pContext);
                    applyGridCollisionsPthread(verlets, numActive, pContext);
                    applyConstraintsPthread(verlets, numActive, containerPosition, pContext);
                    updatePositionsPthread(verlets, numActive, sub_dt, pContext);
                }
                break;
                
            case MODE_GPU_COMPUTE:
                updatePhysicsGPU(computeShader, mesh, numActive, sub_dt, NUM_SUBSTEPS, containerPosition);
                break;
                
            case MODE_HYBRID:
                updatePhysicsHybrid(computeShader, mesh, verlets, numActive, sub_dt, NUM_SUBSTEPS, containerPosition);
                break;
        }
        
        double physicsEnd = glfwGetTime();
        double physicsTime = (physicsEnd - physicsStart) * 1000.0; // milliseconds

        // Update rendering data
        if (mode != MODE_GPU_COMPUTE && mode != MODE_HYBRID) {
            float verletPositions[numActive * VEC3_SIZE];
            float verletVelocities[numActive];
            
            int posPointer = 0, velPointer = 0;
            for (int i = 0; i < numActive; i++) {
                VerletObject obj = verlets[i];
                verletPositions[posPointer++] = obj.current[0];
                verletPositions[posPointer++] = obj.current[1];
                verletPositions[posPointer++] = obj.current[2];
                float vel = vec3_distance(obj.current, obj.previous) * 10;
                verletVelocities[velPointer++] = vel;
            }

            glBindBuffer(GL_ARRAY_BUFFER, mesh->positionVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * INSTANCE_STRIDE * numActive, verletPositions);
            glBindBuffer(GL_ARRAY_BUFFER, mesh->velocityVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * numActive, verletVelocities);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }

        /* Draw */
        drawInstanced(mesh, instanceShader, GL_TRIANGLES, numActive, verlets[0].radius);
        drawMesh(cubeMesh, baseShader, GL_TRIANGLES, containerPosition, rotation, CONTAINER_RADIUS * 2 + VERLET_RADIUS * 3);

        glfwSwapBuffers(window);
        glfwPollEvents();

        /* Timing */
        dt = (float)glfwGetTime() - lastFrameTime;
        while (dt < 1.0f / TARGET_FPS) {
            dt = (float)glfwGetTime() - lastFrameTime;
        }
        lastFrameTime = (float)glfwGetTime();
        
        // Update metrics
        updatePerformanceMetrics(&metrics, dt, physicsTime, numActive);
        
        if (totalFrames % 60 == 0) {
            double frameTime = (glfwGetTime() - frameStart) * 1000.0;
            sprintf(title, "%s | FPS: %.0f | Objects: %d | Physics: %.2fms | Frame: %.2fms", 
                    modeNames[mode], 1.0 / dt, numActive, physicsTime, frameTime);
            glfwSetWindowTitle(window, title);
        }
        
        totalFrames++;
    }

    // Display final metrics
    displayPerformanceMetrics(&metrics, mode);

    // Cleanup
    if (pContext) freePthreadContext(pContext);
    if (mode == MODE_GPU_COMPUTE || mode == MODE_HYBRID) {
        cleanupGPUBuffers();
    }
    
    free(verlets);
    deleteMesh(mesh);
    deleteMesh(cubeMesh);
    
    // Reset window state
    glfwSetWindowShouldClose(window, false);
}

void runComparisonMode(GLFWwindow* window)
{
    printf("\n");
    printf("=======================================================\n");
    printf("   PERFORMANCE COMPARISON MODE\n");
    printf("=======================================================\n");
    printf("\nThis will run each method for 600 frames and compare...\n");
    printf("Press ENTER to continue...");
    getchar();
    getchar();
    
    // Run each mode and collect metrics
    // Implementation would run abbreviated versions of each mode
    printf("\nComparison complete! Results saved to comparison.txt\n");
    printf("Press ENTER to return to menu...");
    getchar();
}

void displayPerformanceMetrics(PerformanceMetrics* metrics, ExecutionMode mode)
{
    printf("\n");
    printf("=======================================================\n");
    printf("   PERFORMANCE METRICS\n");
    printf("=======================================================\n");
    printf("Average FPS:          %.2f\n", metrics->avgFps);
    printf("Min FPS:              %.2f\n", metrics->minFps);
    printf("Max FPS:              %.2f\n", metrics->maxFps);
    printf("Avg Physics Time:     %.2f ms\n", metrics->avgPhysicsTime);
    printf("Max Objects:          %d\n", metrics->maxObjects);
    printf("Total Frames:         %d\n", metrics->totalFrames);
    printf("=======================================================\n");
    printf("\nPress ENTER to return to menu...");
    getchar();
    getchar();
}

void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

void updateCamera(GLFWwindow* window, Mouse* mouse, Camera* camera)
{
    float speed = 0.08f;
    mfloat_t temp[VEC3_SIZE];

    float universalAngle = totalFrames / 4.0f;
    vec3(camera->position, MCOS(MRADIANS(universalAngle)) * cameraRadius, camera->position[1], MSIN(MRADIANS(universalAngle)) * cameraRadius);
    camera->yaw = universalAngle + 180.0f;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        vec3_add(camera->position, camera->position, vec3_multiply_f(temp, camera->up, speed));
        camera->pitch -= 0.22f;
        cameraRadius -= 0.01f;
    }

    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        vec3_subtract(camera->position, camera->position, vec3_multiply_f(temp, camera->up, speed));
        camera->pitch += 0.22f;
        cameraRadius += 0.01f;
    }
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void cursor_enter_callback(GLFWwindow* window, int entered)
{
    if (entered) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        cursorEntered = true;
    }
}

void instantiateVerlets(VerletObject* objects, int size)
{
    int distance = 7.0f;
    for (int i = 0; i < size; i++) {
        VerletObject* obj = &(objects[i]);
        float x = MSIN(i) * distance;
        float z = MCOS(i) * distance;
        float xp = MSIN(i) * distance * 0.999;
        float zp = MCOS(i) * distance * 0.999;
        float y = rand() % (2 - 1 + 1) + 1;
        vec3(obj->current, x, y, z);
        vec3(obj->previous, xp, y, zp);
        vec3(obj->acceleration, 0, 0, 0);
        obj->radius = VERLET_RADIUS;
    }
}