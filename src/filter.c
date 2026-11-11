#include "filter.h"

static int is_vowel(char c) {
    return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' ||
           c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U';
}

size_t remove_vowels(char* text, size_t length) {
    size_t j = 0;

    for (size_t i = 0; i < length; i++) {
        if (!is_vowel(text[i])) text[j++] = text[i];
    }

    return j;
}
