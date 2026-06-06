#include "idGenerator.h"

#include <stdio.h>
#include <string.h>

void generateProcessId(int ordinal, char outId[], size_t outSize) {
    char letters[8];
    int number = ordinal;
    int pos = 0;

    if (!outId || outSize == 0) return;

    while (number > 0 && pos < (int)sizeof(letters) - 1) {
        number--;
        letters[pos++] = (char)('A' + (number % 26));
        number /= 26;
    }

    if (pos == 0) {
        letters[pos++] = 'A';
    }
    letters[pos] = '\0';

    for (int i = 0; i < pos / 2; ++i) {
        char tmp = letters[i];
        letters[i] = letters[pos - i - 1];
        letters[pos - i - 1] = tmp;
    }

    snprintf(outId, outSize, "%s-%03d", letters, ordinal);
    outId[outSize - 1] = '\0';
}
