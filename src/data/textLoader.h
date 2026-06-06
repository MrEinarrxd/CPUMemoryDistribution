#ifndef CpuMemoryTextLoaderH
#define CpuMemoryTextLoaderH

#include "../utils/constants.h"

typedef struct TextRepository {
    char words[MaxRepositoryWords][WordLen];
    int wordCount;
    char phrases[MaxRepositoryPhrases][PhraseLen];
    int phraseCount;
} TextRepository;

int textRepositoryInit(TextRepository* repo, const char* wordPath, const char* phrasePath);
const char* textRepositoryRandomWord(TextRepository* repo);
const char* textRepositoryRandomPhrase(TextRepository* repo);

#endif
