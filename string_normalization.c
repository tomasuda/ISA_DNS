#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

/*
 * Normalize a string by trimming whitespace and converting to lowercase.
 */
char *normalize_string(const char *input) {
    if (input == NULL) return NULL;

    // Trim leading whitespace
    while (isspace((unsigned char)*input)) input++;

    // Trim trailing whitespace
    const char *end = input + strlen(input) - 1;
    while (end > input && isspace((unsigned char)*end)) end--;

    // Create a new string for the normalized output
    size_t length = end - input + 1;
    char *normalized = (char *)malloc(length + 1);
    if (normalized == NULL) return NULL;

    // Copy and convert to lowercase
    for (size_t i = 0; i < length; i++) {
        normalized[i] = tolower((unsigned char)input[i]);
    }
    normalized[length] = '\0';

    return normalized;
}

//afinds all sub dommains and returns upper domains up to main domain
// returns a newly-allocated string containing the suffix after the first dot
// or NULL if there's no suffix. Caller must free() the returned pointer.
char *domain_parser(const char *input)
{
    if (!input) return NULL;
    const char *first_dot = strchr(input, '.');
    if (!first_dot) return NULL;
    const char *suffix = first_dot + 1;
    if (*suffix == '\0') return NULL;

    char *copy = strdup(suffix);
    if (!copy) {
        fprintf(stderr, "strdup in domain_parser");
        return NULL;
    }
    return copy;
}


void to_lower_str(char *string) {
    for (; *string; ++string)
        *string = tolower((unsigned char)*string);
}
