#include "phraseLoader.h"
#include "errorHandler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PHRASES 1000
#define MAX_PHRASE_LEN 500

static char phrases[MAX_PHRASES][MAX_PHRASE_LEN];
static int phraseCount = 0;

int phraseLoaderInit(const char* filePath) {
    FILE* f = fopen(filePath, "r");
    if (!f) {
        errorHandlerLog(ErrorCodeFileOperationFailed, "phraseLoaderInit: no se pudo abrir frases.odt");
        strcpy(phrases[0], "El gato saltó sobre la cerca y corrió velozmente");
        phraseCount = 1;
        return -1;
    }
    char line[MAX_PHRASE_LEN];
    while (fgets(line, sizeof(line), f) && phraseCount < MAX_PHRASES) {
        line[strcspn(line, "\n")] = '\0';
        if (strlen(line) > 0) {
            strncpy(phrases[phraseCount], line, MAX_PHRASE_LEN-1);
            phrases[phraseCount][MAX_PHRASE_LEN-1] = '\0';
            phraseCount++;
        }
    }
    fclose(f);
    if (phraseCount == 0) {
        strcpy(phrases[0], "Frase por defecto para pruebas");
        phraseCount = 1;
    }
    return 0;
}

const char* phraseLoaderGetRandom(void) {
    if (phraseCount == 0) return "Frase de emergencia";
    int idx = rand() % phraseCount;
    return phrases[idx];
}

void phraseLoaderCleanup(void) {}
