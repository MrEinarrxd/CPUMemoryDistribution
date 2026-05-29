#ifndef UNIQUE_ID_H
#define UNIQUE_ID_H

#include "constants.h"
#include <stddef.h>

void uniqueIdInit(void);
void generateProcessId(int processNumber, char* outId, size_t outSize);
int getNumericId(const char* id);
int isValidId(const char* id);
void uniqueIdReset(void);

#endif
