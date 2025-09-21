#include "data_loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hashtable.h"

// Global variable to store the count of valid lines
//Usage for deallocation of memory

// for efectivity of hash table size is doubled
// to reduce collisions
int line_count = 0;

// // Hash function
// unsigned int hash(const char *str) {
//     assert(str != NULL);
//     unsigned int hash = 0;
//     while (*str) {
//         hash = (hash << 5) + *str++;
//     }
//     return hash;
// }

int count_lines(char *file_name) {
    FILE *file = fopen(file_name, "r");
    if (!file) {
        perror("Error opening file");
        return -1;
    }

    char *line = NULL;
    size_t len = 0;
    ssize_t read;

   
    // Read each line from the file
    while ((read = getline(&line, &len, file)) != -1) {
        // Remove newline character if present
        if (read > 0 && line[read - 1] == '\n') {
            line[read - 1] = '\0';
        }
        if(line[0] == '#' || line[0] == '\0') {
            continue; // Skip comments and empty lines
        }
        line_count++;
        // Process the line here (add to your filter structure)
    }
    line_count *= 2;
    // Free the allocated memory
    free(line);
    fclose(file);
    
    return 0;
}



int undesired_domains(char * file_name, ht_table_t table) {

    FILE *file = fopen(file_name, "r");
    if (!file) {
        perror("Error opening file");
        return -1;
    }

    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    while ((read = getline(&line, &len, file)) != -1) {
        // Remove newline character if present
        if (read > 0 && line[read - 1] == '\n') {
            line[read - 1] = '\0';
        }
        // Strip leading whitespaces
        char *start = line;
        while (*start && (*start == ' ' || *start == '\t')) {
            start++;
        }

        // Strip trailing whitespaces
        char *end = start + strlen(start) - 1;
        while (end > start && (*end == ' ' || *end == '\t')) {
            *end = '\0';
            end--;
        }
        if(line[0] == '#' || line[0] == '\0') {
            continue; // Skip comments and empty lines
        }
        
        ht_insert(table, line, line);
        
        // Process the line here (add to your filter structure)
    }

    // Free the allocated memory
    free(line);
    fclose(file);

    // printf("Total valid lines: %d\n", line_count);
    return 0;
}