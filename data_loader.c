#include "data_loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hashtable.h"
#include <unistd.h>
#include <ctype.h>

// Global variable to store the count of valid lines
//Usage for deallocation of memory
// for efectivity of hash table size is doubled
// to reduce collisions
int line_count = 0;


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

int undesired_domains(char *file_name, ht_table_t table) {
    FILE *file = fopen(file_name, "r");
    if (!file) {
        perror("Error opening file");
        return -1;
    }

    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    while ((read = getline(&line, &len, file)) != -1) {
        // Remove trailing newline
        if (read > 0 && line[read - 1] == '\n') {
            line[read - 1] = '\0';
        }

        // Strip leading spaces/tabs
        char *start = line;
        while (*start && isspace((unsigned char)*start)) {
            start++;
        }

        // Strip trailing spaces/tabs
        char *end = start + strlen(start) - 1;
        while (end > start && isspace((unsigned char)*end)) {
            *end = '\0';
            end--;
        }

        // Skip empty lines or comments
        if (*start == '\0' || *start == '#') {
            continue;
        }

        for (size_t i = 0; i < strlen(start); i++) {
            //2 dots beside each other
            if (start[i] == '.' && start[i + 1] == '.') {
                free(line);
                fclose(file);
                return 1;
            }

            if (start[i] == '-') {
                if (i + 1 == strlen(start) || start[i + 1] == '.') {
                    free(line);
                    fclose(file);
                    return 1;
                }
            }
        }


        ht_insert(table, start, start);
    }

    free(line);
    fclose(file);
    return 0; 
}