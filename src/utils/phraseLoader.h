#ifndef PHRASE_LOADER_H
#define PHRASE_LOADER_H

int phraseLoaderInit(const char* filePath);
const char* phraseLoaderGetRandom(void);
void phraseLoaderCleanup(void);

#endif