#include "text_loader.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

void text_loader_init(TextLoader* loader) {
    if (!loader) return;
    memset(loader, 0, sizeof(*loader));
}

static void add_word(TextLoader* loader, const char* start, int len) {
    if (!loader || len <= 0 || loader->word_count >= TEXT_LOADER_MAX_WORDS) return;
    int copy_len = len < SIM_WORD_LEN - 1 ? len : SIM_WORD_LEN - 1;
    memcpy(loader->words[loader->word_count], start, (size_t)copy_len);
    loader->words[loader->word_count][copy_len] = '\0';
    loader->word_count++;
}

int text_loader_load_words(TextLoader* loader, const char* path) {
    if (!loader || !path) return -1;
    FILE* file = fopen(path, "r");
    if (!file) return -1;
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), file)) {
        const char* start = NULL;
        int len = 0;
        for (const char* p = buffer; ; p++) {
            unsigned char ch = (unsigned char)*p;
            if (isalnum(ch)) {
                if (!start) start = p;
                len++;
            } else {
                if (start && len > 0) add_word(loader, start, len);
                start = NULL;
                len = 0;
                if (*p == '\0') break;
            }
        }
    }
    fclose(file);
    if (loader->word_count == 0) {
        strcpy(loader->words[loader->word_count++], "memoria");
        strcpy(loader->words[loader->word_count++], "cpu");
    }
    return 0;
}

int text_loader_load_phrases(TextLoader* loader, const char* path) {
    if (!loader || !path) return -1;
    FILE* file = fopen(path, "r");
    if (!file) return -1;
    char line[SIM_PHRASE_LEN];
    while (fgets(line, sizeof(line), file) && loader->phrase_count < TEXT_LOADER_MAX_PHRASES) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0') continue;
        strncpy(loader->phrases[loader->phrase_count], line, SIM_PHRASE_LEN - 1);
        loader->phrases[loader->phrase_count][SIM_PHRASE_LEN - 1] = '\0';
        loader->phrase_count++;
    }
    fclose(file);
    if (loader->phrase_count == 0) strcpy(loader->phrases[loader->phrase_count++], "cpu memoria proceso pagina swap");
    return 0;
}

const char* text_loader_random_word(TextLoader* loader, RandomSource* random) {
    if (!loader || loader->word_count <= 0) return "memoria";
    return loader->words[random_int(random, 0, loader->word_count - 1)];
}

const char* text_loader_random_phrase(TextLoader* loader, RandomSource* random) {
    if (!loader || loader->phrase_count <= 0) return "cpu memoria proceso pagina swap";
    return loader->phrases[random_int(random, 0, loader->phrase_count - 1)];
}

int text_loader_extract_words(const char* phrase,
                              char out_words[SIM_PHRASE_WORDS][SIM_WORD_LEN],
                              int max_words) {
    if (!phrase || !out_words || max_words <= 0) return 0;
    int count = 0;
    const char* start = NULL;
    int len = 0;
    for (const char* p = phrase; ; p++) {
        unsigned char ch = (unsigned char)*p;
        if (isalnum(ch)) {
            if (!start) start = p;
            len++;
        } else {
            if (start && len > 0 && count < max_words) {
                int copy_len = len < SIM_WORD_LEN - 1 ? len : SIM_WORD_LEN - 1;
                memcpy(out_words[count], start, (size_t)copy_len);
                out_words[count][copy_len] = '\0';
                count++;
            }
            start = NULL;
            len = 0;
            if (*p == '\0' || count >= max_words) break;
        }
    }
    return count;
}
