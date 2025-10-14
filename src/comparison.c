#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "parallel_verlet.h"
#include "verlet.h"

#define COMPARISON_FRAMES 600
#define COMPARISON_OBJECTS 5000

typedef struct {
    const char* name;
    double avgFps;
    double avgPhysicsTime;
    double totalTime;
    double speedup;
    double efficiency;
    int maxObjects;
} ComparisonResult;

void runComparisonTest(VerletObject* objects, int numObjects, int numFrames, 
                       void (*physicsFunc)(VerletObject*, int, float, void*), 
                       void* context, ComparisonResult* result);
void generateComparisonReport(ComparisonResult* results, int numResults);
void plotPerformanceGraph(ComparisonResult* results, int numResults);
double calculateSpeedup(double parallelTime, double sequentialTime);
double calculateEfficiency(double speedup, int numCores);

void runFullComparison()
{
    printf("\n");
    printf("=======================================================\n");
    printf("   PERFORMANCE COMPARISON - STARTING\n");
    printf("=======================================================\n");
    printf("Test Configuration:\n");
    printf("  Objects: %d\n", COMPARISON_OBJECTS);
    printf("  Frames: %d\n", COMPARISON_FRAMES);
    printf("  Substeps: %d\n", NUM_SUBSTEPS);
    printf("\n");
    
    // Initialize objects
    VerletObject* objects = malloc(sizeof(VerletObject) * COMPARISON_OBJECTS);
    for (int i = 0; i < COMPARISON_OBJECTS; i++) {
        float angle = (float)i / COMPARISON_OBJECTS * 6.28318f;
        float radius = 7.0f;
        objects[i].current[0] = cosf(angle) * radius;
        objects[i].current[1] = (rand() % 3) + 1.0f;
        objects[i].current[2] = sinf(angle) * radius;
        objects[i].previous[0] = objects[i].current[0];
        objects[i].previous[1] = objects[i].current[1];
    }
}
