// === src/domain/memory/memoryResize.h ===

#ifndef MEMORY_RESIZE_H
#define MEMORY_RESIZE_H

typedef struct MemoryResizer {
    int resizeAttempts;
    int successfulResizes;
    int currentMemorySize;
    int maximumMemorySize;
} MemoryResizer;

MemoryResizer* memoryResizerCreate(int initialSize, int maxSize);

void memoryResizerDestroy(MemoryResizer* resizer);

int memoryResizerExpandMemory(MemoryResizer* resizer, int newSize);

int memoryResizerShrinkMemory(MemoryResizer* resizer, int newSize);

int memoryResizerGetCurrentSize(MemoryResizer* resizer);

int memoryResizerGetResizeAttempts(MemoryResizer* resizer);

#endif
