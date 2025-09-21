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

char *main_domain_finder(const char *input) {
	if (input == NULL) return NULL;

	// Find the last two segments of the domain
	const char *last_dot = strrchr(input, '.');// last dot (.com)
	if (last_dot == NULL || last_dot == input) return NULL;

	const char *second_last_dot = NULL;
	for (const char *p = last_dot - 1; p >= input; p--) {
		if (*p == '.') {
			second_last_dot = p;
			break;
		}
	}

	if (second_last_dot == NULL) return input;

	// Allocate memory for the main domain
	size_t length = strlen(second_last_dot + 1);
	char *main_domain = (char *)malloc(length + 1);
	if (main_domain == NULL) return NULL;

	strcpy(main_domain, second_last_dot + 1);
	return main_domain;
}

void change_to_dot_format(unsigned char *str) {
	int i, j;
	for(i = 0; i < strlen((const char*)str); ++i) {
		unsigned int len = str[i];
		for(j = 0; j < len; ++j) {
			str[i] = str[i + 1];
			++i;
		}
		str[i] = '.';
	}
	str[i - 1] = '\0';
}

// The function converts the dot-based hostname into the DNS format www.apple.com -> 3www5apple3com0
void change_to_dns_format(char *src, unsigned char *dest) {
	int pos = 0;
	int len = 0;
	int i;
	strcat(src, ".");
	for(i = 0; i < (int)strlen(src); ++i) {
		if(src[i] == '.') {
			dest[pos] = i - len;
			++pos;
			for(; len < i; ++len) {
				dest[pos] = src[len];
				++pos;
			}
			len++;
		}
	}
	dest[pos] = '\0';
}