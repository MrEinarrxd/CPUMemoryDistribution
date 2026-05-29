#include "uniqueId.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

void generateProcessId(int processNumber, char* outId, size_t outSize) {
    if (!outId || outSize == 0) return;
    if (processNumber < 1) processNumber = 1;
    char letter;
    int num;
    if (processNumber <= 100) {
        letter = 'A';
        num = processNumber;
    } else if (processNumber <= 200) {
        letter = 'B';
        num = processNumber - 100;
    } else {
        letter = 'C';
        num = processNumber - 200;
    }
    snprintf(outId, outSize, "%c-%d", letter, num);
}

int getNumericId(const char* id) {
    if (!id) return -1;
    if (strlen(id) < 3 || id[1] != '-') return -1;
    char letter = toupper(id[0]);
    int num = 0;
    if (sscanf(id + 2, "%d", &num) != 1) return -1;
    if (letter == 'A') return num;
    if (letter == 'B') return 100 + num;
    if (letter == 'C') return 200 + num;
    return -1;
}

int isValidId(const char* id) {
    int n = getNumericId(id);
    return (n >= 1 && n <= totalProcesos);
}

void uniqueIdInit(void) {}
void uniqueIdReset(void) {}
