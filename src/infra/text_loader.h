#ifndef INFRA_TEXT_LOADER_H
#define INFRA_TEXT_LOADER_H

#include "../simulation/config.h"
#include "random.h"

#define TEXT_LOADER_MAX_WORDS 4096
#define TEXT_LOADER_MAX_PHRASES 512

typedef struct TextLoader {
    char words[TEXT_LOADER_MAX_WORDS][SIM_WORD_LEN];
    int word_count;
    char phrases[TEXT_LOADER_MAX_PHRASES][SIM_PHRASE_LEN];
    int phrase_count;
} TextLoader;

void text_loader_init(TextLoader* loader);
int text_loader_load_words(TextLoader* loader, const char* path);
int text_loader_load_phrases(TextLoader* loader, const char* path);
const char* text_loader_random_word(TextLoader* loader, RandomSource* random);
const char* text_loader_random_phrase(TextLoader* loader, RandomSource* random);
int text_loader_extract_words(const char* phrase,
                              char out_words[SIM_PHRASE_WORDS][SIM_WORD_LEN],
                              int max_words);

#endif
