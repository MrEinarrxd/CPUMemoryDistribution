// === src/domain/memory/bitmapManager.h ===

#ifndef BITMAP_MANAGER_H
#define BITMAP_MANAGER_H

#include "../../utils/constants.h"

// Algoritmo bitmap para memoria SIN swap
// Cada bit representa un bloque de memoria: 0=libre, 1=ocupado
typedef struct BitmapManager {
    unsigned char* bitmap;                              // Arreglo bitmap, tamaño maxMarcos/8
    int totalBlocks;                                    // = maxMarcos
    int blockSize;                                      // Tamaño de cada bloque en bytes
    int usedBlocks;                                     // Número de bloques en uso actualmente
    int internalWaste;                                  // Fragmentación interna
    int externalWaste;                                  // Fragmentación externa
    int totalAllocations;                               // Contador de operaciones de asignación
    int totalDeallocations;                             // Contador de operaciones de liberación
    int processBlocks[procesosEnEjecucion];             // Bloques asignados por proceso
    int processStartBlock[procesosEnEjecucion];   // Bloque inicial asignado a cada proceso
} BitmapManager;

// Crea administrador bitmap con el tamaño de bloque especificado
BitmapManager* bitmapManagerCreate(int blockSize);

// Destruye y libera recursos del administrador bitmap
void bitmapManagerDestroy(BitmapManager* manager);

// Asigna bloques para proceso, retorna índice inicial o -1 en fallo
int bitmapManagerAllocate(BitmapManager* manager, int processIndex, int blocksNeeded);

// Libera bloques de proceso
void bitmapManagerFree(BitmapManager* manager, int processIndex);

// Obtiene número actual de bloques usados
int bitmapManagerGetUsedBlocks(BitmapManager* manager);

// Obtiene número actual de bloques libres
int bitmapManagerGetFreeBlocks(BitmapManager* manager);

// Obtiene valor de fragmentación interna
int bitmapManagerGetInternalWaste(BitmapManager* manager);

// Obtiene valor de fragmentación externa
int bitmapManagerGetExternalWaste(BitmapManager* manager);

// Imprime estado actual del administrador bitmap
void bitmapManagerPrint(BitmapManager* manager);

#endif
