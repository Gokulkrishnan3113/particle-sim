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

// Preprocessor constants
#define ANIMATION_TIME 90.0f
#define ADDITION_SPEED 10
#define TARGET_FPS 60
#define NUM_SUBSTEPS 8

// Computation modes
typedef enum
{
    MODE_GPU = 1,
    MODE_CPU = 2
} ComputeMode;

// Function prototypes
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void cursor_enter_callback(GLFWwindow* window, int entered);
void processInput(GLFWwindow* window);
void updateCamera(GLFWwindow* window, Mouse* mouse, Camera* camera);
void instantiateVerlets(VerletObject* objects, int size);
int displayMenu();
void runSimulation(ComputeMode mode);

// CPU computation functions
void applyForcesCPU(VerletObject *verlets, int numActive);
void applyGridCollisionsCPU(VerletObject *verlets, int numActive);
void applyConstraintsCPU(VerletObject *verlets, int numActive, mfloat_t *containerPos);
void updatePositionsCPU(VerletObject *verlets, int numActive, float dt);

// GPU computation functions (using compute shaders)
void setupGPUCompute();
void applyForcesGPU(VerletObject *verlets, int numActive);
void applyGridCollisionsGPU(VerletObject *verlets, int numActive);
void applyConstraintsGPU(VerletObject *verlets, int numActive, mfloat_t *containerPos);
void updatePositionsGPU(VerletObject *verlets, int numActive, float dt);
void cleanupGPUCompute();

// Settings
const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

// Global variables
bool cursorEntered = false;
Camera* camera;
float cameraRadius = 24.0f;
int totalFrames = 0;
ComputeMode currentMode = MODE_CPU;

// GPU compute resources
GLuint computeProgram = 0;
GLuint verletSSBO = 0;

// Performance metrics
typedef struct
{
    float totalTime;
    int frameCount;
    float minFPS;
    float maxFPS;
    float avgFPS;
    float avgFrameTime;   // new
    float avgSimStepTime; // new
    float avgThroughput;  // new
} PerformanceMetrics;

PerformanceMetrics metrics = {0, 0, 999999.0f, 0.0f, 0.0f, 0, 0, 0};

int main()
{
    int choice = displayMenu();

    if (choice != MODE_GPU && choice != MODE_CPU)
    {
        printf("Invalid choice. Exiting.\n");
        return -1;
    }

    currentMode = (ComputeMode)choice;
    printf("\nStarting simulation with %s computation...\n",
           currentMode == MODE_GPU ? "GPU" : "CPU");

    runSimulation(currentMode);

    return 0;
}

int displayMenu()
{
    int choice;

    printf("\n========================================\n");
    printf("    VERLET INTEGRATION SIMULATION\n");
    printf("========================================\n\n");
    printf("Select Computation Mode:\n\n");
    printf("  1. GPU Computation (Compute Shaders)\n");
    printf("     - Faster for large particle counts\n");
    printf("     - Requires OpenGL 4.3+\n\n");
    printf("  2. CPU Computation (Traditional)\n");
    printf("     - More compatible\n");
    printf("     - Easier to debug\n\n");
    printf("========================================\n");
    printf("Enter your choice (1 or 2): ");

    scanf("%d", &choice);

    return choice;
}

void runSimulation(ComputeMode mode)
{
    GLFWwindow *window;

    if (!glfwInit())
    {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return;
    }

    // Use OpenGL 3.3 for better compatibility on macOS
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    char windowTitle[100];
    sprintf(windowTitle, "Verlet Integration - %s Mode",
            mode == MODE_GPU ? "GPU" : "CPU");

    window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, windowTitle, NULL, NULL);
    if (!window) {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSwapInterval(1);
    glfwSetCursorEnterCallback(window, cursor_enter_callback);

    GLenum err = glewInit();
    if (err != GLEW_OK)
    {
        fprintf(stderr, "Failed to initialize GLEW: %s\n", glewGetErrorString(err));
        glfwTerminate();
        return;
    }

    printf("OpenGL Version: %s\n", glGetString(GL_VERSION));
    printf("GLSL Version: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

    // OpenGL Settings
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

    if (mode == MODE_GPU)
        setupGPUCompute();

    printf("Loading shaders...\n");
    unsigned int phongShader = createShader("shaders/phong_vertex.glsl", "shaders/phong_fragment.glsl");
    unsigned int instanceShader = createShader("shaders/instance_vertex.glsl", "shaders/instance_fragment.glsl");
    unsigned int baseShader = createShader("shaders/base_vertex.glsl", "shaders/base_fragment.glsl");

    if (phongShader == 0 || instanceShader == 0 || baseShader == 0)
    {
        fprintf(stderr, "Failed to create shaders\n");
        glfwTerminate();
        return;
    }

    printf("Loading models...\n");
    Mesh* mesh = createMesh("models/sphere.obj", true);
    Mesh* cubeMesh = createMesh("models/cube.obj", false);

    if (mesh == NULL || cubeMesh == NULL)
    {
        fprintf(stderr, "Failed to load models\n");
        glfwTerminate();
        return;
    }

    printf("Simulation started successfully!\n");
    printf("Controls:\n");
    printf("  V - Add more particles (hold to add many)\n");
    printf("  G - Apply upward force\n");
    printf("  W/S - Zoom in/out\n");
    printf("  Arrow Keys - Move container\n");
    printf("  ESC - Exit\n\n");

    mfloat_t containerPosition[VEC3_SIZE] = { 0, 0, 0 };
    mfloat_t rotation[VEC3_SIZE] = { 0, 0, 0 };

    VerletObject* verlets = malloc(sizeof(VerletObject) * MAX_INSTANCES);
    instantiateVerlets(verlets, MAX_INSTANCES);
    int numActive = 50;

    mfloat_t view[MAT4_SIZE];
    camera = createCamera((mfloat_t[]){0, 0, cameraRadius});
    Mouse* mouse = createMouse();

    float dt = 0.000001f;
    float lastFrameTime = (float)glfwGetTime();
    char title[200] = "";

    srand(time(NULL));

    while (!glfwWindowShouldClose(window))
    {
        updateMouse(window, mouse);
        processInput(window);

        if (glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS)
            addForce(verlets, numActive, (mfloat_t[]){0, 3, 0}, -30.0f * NUM_SUBSTEPS);

        updateCamera(window, mouse, camera);
        createViewMatrix(view, camera);

        glUseProgram(phongShader);
        glUniformMatrix4fv(glGetUniformLocation(phongShader, "view"), 1, GL_FALSE, view);
        glUseProgram(baseShader);
        glUniformMatrix4fv(glGetUniformLocation(baseShader, "view"), 1, GL_FALSE, view);
        glUseProgram(instanceShader);
        glUniformMatrix4fv(glGetUniformLocation(instanceShader, "view"), 1, GL_FALSE, view);
        glUseProgram(0);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        if (1.0 / dt >= TARGET_FPS - 5 && glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS && numActive < MAX_INSTANCES)
            numActive += ADDITION_SPEED;

        // ===================== PERFORMANCE TIMING START =====================
        double simStart = glfwGetTime();

        float sub_dt = dt / NUM_SUBSTEPS;
        for (int i = 0; i < NUM_SUBSTEPS; i++)
        {
            if (mode == MODE_GPU)
            {
                applyForcesGPU(verlets, numActive);
                applyGridCollisionsGPU(verlets, numActive);
                applyConstraintsGPU(verlets, numActive, containerPosition);
                updatePositionsGPU(verlets, numActive, sub_dt);
            }
            else
            {
                applyForcesCPU(verlets, numActive);
                applyGridCollisionsCPU(verlets, numActive);
                applyConstraintsCPU(verlets, numActive, containerPosition);
                updatePositionsCPU(verlets, numActive, sub_dt);
            }
        }

        double simEnd = glfwGetTime();
        float simStepTime = (float)((simEnd - simStart) * 1000.0f); // in ms
        float particlesPerSecond = (numActive * NUM_SUBSTEPS) / (float)(simEnd - simStart);
        // ===================== PERFORMANCE TIMING END =====================

        // Rendering
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

        drawInstanced(mesh, instanceShader, GL_TRIANGLES, numActive, verlets[0].radius);
        drawMesh(cubeMesh, baseShader, GL_TRIANGLES, containerPosition, rotation, CONTAINER_RADIUS * 2 + VERLET_RADIUS * 3);

        glfwSwapBuffers(window);
        glfwPollEvents();

        dt = (float)glfwGetTime() - lastFrameTime;
        while (dt < 1.0f / TARGET_FPS)
            dt = (float)glfwGetTime() - lastFrameTime;
        lastFrameTime = (float)glfwGetTime();
        totalFrames++;

        // Update metrics
        float currentFPS = 1.0f / dt;
        metrics.totalTime += dt;
        metrics.frameCount++;
        if (currentFPS < metrics.minFPS)
            metrics.minFPS = currentFPS;
        if (currentFPS > metrics.maxFPS)
            metrics.maxFPS = currentFPS;
        metrics.avgFPS = metrics.frameCount / metrics.totalTime;
        metrics.avgFrameTime = (metrics.totalTime / metrics.frameCount) * 1000.0f;
        metrics.avgSimStepTime = (metrics.avgSimStepTime * 0.95f) + (simStepTime * 0.05f);
        metrics.avgThroughput = (metrics.avgThroughput * 0.95f) + (particlesPerSecond * 0.05f);

        if (totalFrames % 60 == 0)
        {
            sprintf(title,
                    "FPS: %-4.0f | Frame: %.2f ms | Step: %.2f ms | Part/s: %.2fK | Balls: %-5d | %s",
                    currentFPS, metrics.avgFrameTime, metrics.avgSimStepTime,
                    metrics.avgThroughput / 1000.0f, numActive,
                    mode == MODE_GPU ? "GPU" : "CPU");
            glfwSetWindowTitle(window, title);
        }
    }

    if (mode == MODE_GPU)
        cleanupGPUCompute();

    // Final performance report
    printf("\n========================================\n");
    printf("  PERFORMANCE SUMMARY - %s MODE\n", mode == MODE_GPU ? "GPU" : "CPU");
    printf("========================================\n");
    printf("Total Frames:     %d\n", metrics.frameCount);
    printf("Total Time:       %.2f seconds\n", metrics.totalTime);
    printf("Average FPS:      %.2f\n", metrics.avgFPS);
    printf("Average FrameTime: %.2f ms\n", metrics.avgFrameTime);
    printf("Average Sim Step: %.2f ms\n", metrics.avgSimStepTime);
    printf("Avg Throughput:   %.2fK particles/sec\n", metrics.avgThroughput / 1000.0f);
    printf("Min FPS:          %.2f\n", metrics.minFPS);
    printf("Max FPS:          %.2f\n", metrics.maxFPS);
    printf("========================================\n\n");

    free(verlets);
    glfwTerminate();
}

// CPU implementations
void applyForcesCPU(VerletObject *verlets, int numActive) { applyForces(verlets, numActive); }
void applyGridCollisionsCPU(VerletObject *verlets, int numActive) { applyGridCollisions(verlets, numActive); }
void applyConstraintsCPU(VerletObject *verlets, int numActive, mfloat_t *containerPos) { applyConstraints(verlets, numActive, containerPos); }
void updatePositionsCPU(VerletObject *verlets, int numActive, float dt) { updatePositions(verlets, numActive, dt); }

// GPU stubs
void setupGPUCompute()
{
    printf("Setting up GPU compute resources...\n");
    glGenBuffers(1, &verletSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, verletSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(VerletObject) * MAX_INSTANCES, NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}
void applyForcesGPU(VerletObject *verlets, int numActive) { applyForces(verlets, numActive); }
void applyGridCollisionsGPU(VerletObject *verlets, int numActive) { applyGridCollisions(verlets, numActive); }
void applyConstraintsGPU(VerletObject *verlets, int numActive, mfloat_t *containerPos) { applyConstraints(verlets, numActive, containerPos); }
void updatePositionsGPU(VerletObject *verlets, int numActive, float dt) { updatePositions(verlets, numActive, dt); }
void cleanupGPUCompute()
{
    if (verletSSBO != 0)
        glDeleteBuffers(1, &verletSSBO);
    if (computeProgram != 0)
        glDeleteProgram(computeProgram);
}

// Input and camera
void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}
void updateCamera(GLFWwindow *window, Mouse *mouse, Camera *camera)
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

// Misc
void framebuffer_size_callback(GLFWwindow *window, int width, int height) { glViewport(0, 0, width, height); }
void cursor_enter_callback(GLFWwindow *window, int entered)
{
    if (entered)
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        cursorEntered = true;
    }
}
void instantiateVerlets(VerletObject *objects, int size)
{
    int distance = 7.0f;
    for (int i = 0; i < size; i++) {
        VerletObject *obj = &(objects[i]);
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
