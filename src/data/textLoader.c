#include "textLoader.h"
#include "../utils/randomUtils.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static void normalizeToken(char* token) {
    int readPos = 0;
    int writePos = 0;
    while (token[readPos] != '\0') {
        unsigned char ch = (unsigned char)token[readPos++];
        if (isalnum(ch) || ch == '_' || ch == '-') {
            token[writePos++] = (char)tolower(ch);
        }
    }
    token[writePos] = '\0';
}

static int loadWords(TextRepository* repo, const char* path) {
    FILE* file = fopen(path, "r");
    char token[WordLen];

    if (!file) return -1;
    repo->wordCount = 0;
    while (repo->wordCount < MaxRepositoryWords && fscanf(file, "%31s", token) == 1) {
        normalizeToken(token);
        if (token[0] == '\0') continue;
        strncpy(repo->words[repo->wordCount], token, WordLen - 1);
        repo->words[repo->wordCount][WordLen - 1] = '\0';
        repo->wordCount++;
    }
    fclose(file);
    return repo->wordCount > 0 ? 0 : -1;
}

static int loadPhrases(TextRepository* repo, const char* path) {
    FILE* file = fopen(path, "r");
    char line[PhraseLen];

    if (!file) return -1;
    repo->phraseCount = 0;
    while (repo->phraseCount < MaxRepositoryPhrases && fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0') continue;
        strncpy(repo->phrases[repo->phraseCount], line, PhraseLen - 1);
        repo->phrases[repo->phraseCount][PhraseLen - 1] = '\0';
        repo->phraseCount++;
    }
    fclose(file);
    return repo->phraseCount > 0 ? 0 : -1;
}

int textRepositoryInit(TextRepository* repo, const char* wordPath, const char* phrasePath) {
    if (!repo || !wordPath || !phrasePath) return -1;
    memset(repo, 0, sizeof(*repo));

    if (loadWords(repo, wordPath) != 0) {
        return -1;
    }

    if (loadPhrases(repo, phrasePath) != 0) {
        return -1;
    }

    return 0;
}

const char* textRepositoryRandomWord(TextRepository* repo) {
    if (!repo || repo->wordCount <= 0) return NULL;
    return repo->words[randomInt(0, repo->wordCount - 1)];
}

const char* textRepositoryRandomPhrase(TextRepository* repo) {
    if (!repo || repo->phraseCount <= 0) return NULL;
    return repo->phrases[randomInt(0, repo->phraseCount - 1)];
}
