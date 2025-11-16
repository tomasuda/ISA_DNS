/**
 * @file hashtable.h
 * @author Tomas Suda (xsudato00)
 * @date 20/9/2025
 */
// reused most of code from IAL project 2024/25
#ifndef IAL_HASHTABLE_H
#define IAL_HASHTABLE_H

#include <stdbool.h>


typedef struct ht_item {
  char *key;
  char *value;
  struct ht_item *next;
} ht_item_t;

//table
typedef ht_item_t **ht_table_t;

int get_hash(char *key);
void ht_init(ht_table_t table);
ht_item_t *ht_search(ht_table_t table, char *key);
void ht_insert(ht_table_t table, char *key, char *data);
char *ht_get(ht_table_t table, char *key);
void ht_delete_all(ht_table_t table);

#endif
