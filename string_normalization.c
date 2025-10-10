#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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
char * domain_parser(char* input)
{
	if (input==NULL) return NULL;
	char *first_dot = strchr(input, '.');// first dot subsud.sub.example.com -> sub.example.com
	if (first_dot == NULL) return NULL;
	return first_dot + 1;

}

void to_lower_str(char *s) {
    for (; *s; ++s)
        *s = tolower((unsigned char)*s);
}

// char *main_domain_finder( char *input) {
// 	if (input == NULL) return NULL;

// 	// Find the last two segments of the domain
// 	const char *last_dot = strrchr(input, '.');// last dot (.com)
// 	if (last_dot == NULL || last_dot == input) return NULL;

// 	const char *second_last_dot = NULL;
// 	for (const char *p = last_dot - 1; p >= input; p--) {
// 		if (*p == '.') {
// 			second_last_dot = p;
// 			break;
// 		}
// 	}

// 	if (second_last_dot == NULL) return input;

// 	// Allocate memory for the main domain
// 	size_t length = strlen(second_last_dot + 1);
// 	char *main_domain = (char *)malloc(length + 1);
// 	if (main_domain == NULL) return NULL;

// 	strcpy(main_domain, second_last_dot + 1);
// 	return main_domain;
// }