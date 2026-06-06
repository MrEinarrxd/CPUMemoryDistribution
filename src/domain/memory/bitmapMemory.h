#ifndef CpuMemoryBitmapMemoryH
#define CpuMemoryBitmapMemoryH

#include "../../utils/constants.h"

typedef struct BitmapMemory {
    unsigned char used[PhysicalFrameCount];
    int ownerProcess[PhysicalFrameCount];
    int ownerPage[PhysicalFrameCount];
    int usedFrames;
    int freeFrames;
    int largestFreeRun;
    int freeRunCount;
    int externalWaste;
    float fragmentation;
} BitmapMemory;

void bitmapMemoryInit(BitmapMemory* memory);
int bitmapMemoryAllocateFrame(BitmapMemory* memory, int processIndex, int pageIndex);
void bitmapMemoryFreeFrame(BitmapMemory* memory, int frameIndex);
void bitmapMemoryUpdateMetrics(BitmapMemory* memory);

#endif
