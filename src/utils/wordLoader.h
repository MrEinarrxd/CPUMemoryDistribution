// === src/utils/wordLoader.h ===

#ifndef WORD_LOADER_H
#define WORD_LOADER_H

int wordLoaderInit(const char* filePath);

const char* wordLoaderGetRandom(void);

int wordLoaderGetCount(void);

void wordLoaderCleanup(void);

#endif
