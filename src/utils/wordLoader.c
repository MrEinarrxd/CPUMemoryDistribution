#include "wordLoader.h"
#include "errorHandler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_WORDS 5000
#define MAX_WORD_LEN 64

static char words[MAX_WORDS][MAX_WORD_LEN];
static int wordCount = 0;

int wordLoaderInit(const char* filePath) {
    if (!filePath) {
        errorHandlerLog(ErrorCodeInvalidArgument, "wordLoaderInit: filePath es NULL");
        return -1;
    }
    
    FILE* f = fopen(filePath, "r");
    if (!f) {
        errorHandlerLog(ErrorCodeFileOperationFailed, "wordLoaderInit: no se pudo abrir libro1.odt");
        // Cargar palabras por defecto
        const char* defaultWords[] = {
            "proceso", "memoria", "sistema", "cpu", "io", "scheduler", "queue", 
            "algoritmo", "ejecucion", "contexto", "ciclo", "pagina", "bloque",
            "tabla", "buffer", "cache", "tarea", "tiempo", "quantum", "prioridad"
        };
        wordCount = sizeof(defaultWords) / sizeof(defaultWords[0]);
        for (int i = 0; i < wordCount; i++) {
            strncpy(words[i], defaultWords[i], MAX_WORD_LEN - 1);
            words[i][MAX_WORD_LEN - 1] = '\0';
        }
        return -1;
    }
    
    char line[MAX_WORD_LEN];
    wordCount = 0;
    while (fgets(line, sizeof(line), f) && wordCount < MAX_WORDS) {
        // Remover espacios en blanco y saltos de línea
        line[strcspn(line, "\n")] = '\0';
        line[strcspn(line, "\r")] = '\0';
        
        // Remover espacios al inicio y final
        int start = 0;
        while (line[start] == ' ' || line[start] == '\t') start++;
        
        if (line[start] != '\0') {
            strncpy(words[wordCount], line + start, MAX_WORD_LEN - 1);
            words[wordCount][MAX_WORD_LEN - 1] = '\0';
            wordCount++;
        }
    }
    
    fclose(f);
    
    if (wordCount == 0) {
        // Palabras por defecto si el archivo está vacío
        const char* defaultWords[] = {
            "proceso", "memoria", "sistema", "cpu", "io"
        };
        wordCount = sizeof(defaultWords) / sizeof(defaultWords[0]);
        for (int i = 0; i < wordCount; i++) {
            strncpy(words[i], defaultWords[i], MAX_WORD_LEN - 1);
            words[i][MAX_WORD_LEN - 1] = '\0';
        }
        return -1;
    }
    
    return 0;
}

const char* wordLoaderGetRandom(void) {
    if (wordCount == 0) return "default";
    int idx = rand() % wordCount;
    return words[idx];
}

int wordLoaderGetCount(void) {
    return wordCount;
}

void wordLoaderCleanup(void) {
    wordCount = 0;
}
