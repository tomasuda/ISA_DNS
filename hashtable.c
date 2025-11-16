/**
 * @file hashtable.c
 * @author Tomas Suda (xsudato00)
 * @date 20/9/2025
 */
// reused most of code from IAL project 2024/25
#include "hashtable.h"
#include "data_loader.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Hash function returning an index in the range <0, HT_SIZE-1>
int get_hash(char *key) {
    int result = 1;
    int length = strlen(key);
    for (int i = 0; i < length; i++) {
        result += key[i];
    }
    return (result % line_count);
}

// Initialize the hash table
void ht_init(ht_table_t table) {
    for (int i = 0; i < line_count; i++) {
        table[i] = NULL;
    }
}

// Search for an item in the table
ht_item_t *ht_search(ht_table_t table, char *key) {
    if (key == NULL || table == NULL)
        return NULL;

    int index = get_hash(key) % line_count;
    ht_item_t *item = table[index];

    while (item != NULL) {
        if (strcmp(item->key, key) == 0) {
            return item; // item found
        }
        item = item->next;
    }

    return NULL; // key not found
}

// Insert a new item or update existing item
void ht_insert(ht_table_t table, char *key, char *value) {
    if (key == NULL || value == NULL || table == NULL) return;

    ht_item_t *item = ht_search(table, key);

    if (item == NULL) {
        int index = get_hash(key) % line_count;

        ht_item_t *new_item = (ht_item_t *)malloc(sizeof(ht_item_t));
        if (new_item == NULL) return;

        new_item->key = (char *)malloc(strlen(key) + 1);
        if (new_item->key == NULL) {
            free(new_item);
            return;
        }
        strcpy(new_item->key, key);

        new_item->value = (char *)malloc(strlen(value) + 1);
        if (new_item->value == NULL) {
            free(new_item->key);
            free(new_item);
            return;
        }
        strcpy(new_item->value, value);

        new_item->next = table[index];
        table[index] = new_item;
    } else {
        char *new_value = (char *)malloc(strlen(value) + 1);
        if (new_value == NULL) return;

        strcpy(new_value, value);
        free(item->value);
        item->value = new_value;
    }
}

// Get the value associated with a key
char *ht_get(ht_table_t table, char *key) {
    if (key == NULL || table == NULL) return NULL;

    ht_item_t *item = ht_search(table, key);
    if (item == NULL) return NULL;

    return item->value;
}

// Delete all items in the table
void ht_delete_all(ht_table_t table) {
    if (table == NULL) return;

    for (int i = 0; i < line_count; i++) {
        ht_item_t *item = table[i];

        while (item != NULL) {
            ht_item_t *next_item = item->next;
            free(item->key);
            free(item->value);
            free(item);
            item = next_item;
        }

        table[i] = NULL;
    }
    free(table);
    table = NULL;
}