// src/utils/wordLoader.c
#include "wordLoader.h"
#include "errorHandler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_WORDS 100000

static char** words = NULL;
static int wordCount = 0;
static FILE* file = NULL;

static void extractWordsFromLine(char* line) {
    char* token = strtok(line, " \t\n\r,.;:!?\"'()[]{}<>");
    while (token != NULL) {
        char* start = token;
        while (*start && !isalnum((unsigned char)*start)) start++;
        if (*start == '\0') {
            token = strtok(NULL, " \t\n\r,.;:!?\"'()[]{}<>");
            continue;
        }
        char* end = start + strlen(start) - 1;
        while (end > start && !isalnum((unsigned char)*end)) end--;
        *(end+1) = '\0';

        if (wordCount < MAX_WORDS) {
            words[wordCount] = strdup(start);
            if (words[wordCount]) wordCount++;
        } else break;
        token = strtok(NULL, " \t\n\r,.;:!?\"'()[]{}<>");
    }
}

int wordLoaderInit(const char* filePath) {
    if (!filePath) {
        errorHandlerLog(ErrorCodeInvalidArgument, "wordLoaderInit: ruta de archivo nula");
        return -1;
    }
    file = fopen(filePath, "r");
    if (!file) {
        errorHandlerLog(ErrorCodeFileOperationFailed, "wordLoaderInit: no se pudo abrir el archivo");
        return -1;
    }
    words = (char**)calloc(MAX_WORDS, sizeof(char*));
    if (!words) {
        fclose(file);
        file = NULL;
        errorHandlerLog(ErrorCodeMemoryAllocationFailed, "wordLoaderInit: fallo al asignar memoria para words");
        return -1;
    }
    char line[1024];
    while (fgets(line, sizeof(line), file))
        extractWordsFromLine(line);
    fclose(file);
    file = NULL;
    if (wordCount == 0) {
        words[0] = strdup("default");
        wordCount = 1;
    }
    return 0;
}

const char* wordLoaderGetRandom(void) {
    if (!words || wordCount == 0) return "error";
    int idx = rand() % wordCount;
    return words[idx];
}

int wordLoaderGetCount(void) { return wordCount; }

void wordLoaderCleanup(void) {
    if (words) {
        for (int i = 0; i < wordCount; i++) free(words[i]);
        free(words);
        words = NULL;
    }
    wordCount = 0;
    if (file) { fclose(file); file = NULL; }
}
