#include "bitmapMemory.h"

#include <string.h>

void bitmapMemoryInit(BitmapMemory* memory) {
    if (!memory) return;
    memset(memory, 0, sizeof(*memory));
    for (int i = 0; i < PhysicalFrameCount; ++i) {
        memory->ownerProcess[i] = -1;
        memory->ownerPage[i] = -1;
    }
    bitmapMemoryUpdateMetrics(memory);
}

int bitmapMemoryAllocateFrame(BitmapMemory* memory, int processIndex, int pageIndex) {
    if (!memory) return -1;
    for (int i = 0; i < PhysicalFrameCount; ++i) {
        if (!memory->used[i]) {
            memory->used[i] = 1;
            memory->ownerProcess[i] = processIndex;
            memory->ownerPage[i] = pageIndex;
            bitmapMemoryUpdateMetrics(memory);
            return i;
        }
    }
    return -1;
}

void bitmapMemoryFreeFrame(BitmapMemory* memory, int frameIndex) {
    if (!memory || frameIndex < 0 || frameIndex >= PhysicalFrameCount) return;
    if (!memory->used[frameIndex]) return;
    memory->used[frameIndex] = 0;
    memory->ownerProcess[frameIndex] = -1;
    memory->ownerPage[frameIndex] = -1;
    bitmapMemoryUpdateMetrics(memory);
}

void bitmapMemoryUpdateMetrics(BitmapMemory* memory) {
    int currentRun = 0;
    int freeFrames = 0;
    int runCount = 0;
    int inRun = 0;

    if (!memory) return;
    memory->usedFrames = 0;
    memory->largestFreeRun = 0;

    for (int i = 0; i < PhysicalFrameCount; ++i) {
        if (memory->used[i]) {
            memory->usedFrames++;
            currentRun = 0;
            inRun = 0;
        } else {
            freeFrames++;
            currentRun++;
            if (!inRun) {
                runCount++;
                inRun = 1;
            }
            if (currentRun > memory->largestFreeRun) {
                memory->largestFreeRun = currentRun;
            }
        }
    }

    memory->freeFrames = freeFrames;
    memory->freeRunCount = runCount;
    memory->externalWaste = freeFrames > memory->largestFreeRun
        ? freeFrames - memory->largestFreeRun
        : 0;
    memory->fragmentation = freeFrames > 0
        ? (float)memory->externalWaste / (float)freeFrames
        : 0.0f;
}
