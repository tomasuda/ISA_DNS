/*
 * Tabulka s rozptýlenými položkami
 *
 * S využitím datových typů ze souboru hashtable.h a připravených koster
 * funkcí implementujte tabulku s rozptýlenými položkami s explicitně
 * zretězenými synonymy.
 *
 * Při implementaci uvažujte velikost tabulky HT_SIZE.
 */

#include "hashtable.h"
#include "data_loader.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// int HT_SIZE = line_count;

/*
 * Rozptylovací funkce která přidělí zadanému klíči index z intervalu
 * <0,HT_SIZE-1>. Ideální rozptylovací funkce by měla rozprostírat klíče
 * rovnoměrně po všech indexech. Zamyslete sa nad kvalitou zvolené funkce.
 */
int get_hash(char *key) {
  int result = 1;
  int length = strlen(key);
  for (int i = 0; i < length; i++) {
    result += key[i];
  }
  return (result % line_count);
}

/*
 * Inicializace tabulky — zavolá sa před prvním použitím tabulky.
 */
void ht_init(ht_table_t table) {

    // Inicializace všech položek tabulky na NULL
    //  printf("inicializuji tabulku o velikosti %d\n", line_count);
   for (int i = 0; i < line_count; i++) {
      // printf("inicializuji index %d\n", i);
        table[i] = NULL;
    }
}

/*
 * Vyhledání prvku v tabulce.
 *
 * V případě úspěchu vrací ukazatel na nalezený prvek; v opačném případě vrací
 * hodnotu NULL.
 */
ht_item_t *ht_search(ht_table_t table, char *key) {
    if (key == NULL || table == NULL)
        return NULL;

    int index = get_hash(key) % line_count;
    ht_item_t *item = (table)[index];

    while (item != NULL) {
        if (strcmp(item->key, key) == 0) {
            return item;  // našel se prvek
        }
        // Debug: kolize, jdeme dál
        //  printf("collision: hledám %s, aktuální klíč %s\n", key, item->key);
        item = item->next;
    }

    return NULL;  // klíč se nenašel
}


/*
 * Vložení nového prvku do tabulky.
 *
 * Pokud prvek s daným klíčem už v tabulce existuje, nahraďte jeho hodnotu.
 *
 * Při implementaci využijte funkci ht_search. Pri vkládání prvku do seznamu
 * synonym zvolte nejefektivnější možnost a vložte prvek na začátek seznamu.
 */
void ht_insert(ht_table_t table, char *key, char *value) {
 
    if (key == NULL || value == NULL || table == NULL) return;
    
    ht_item_t *item = ht_search(table, key);
    
    if (item == NULL) {
      
        int index = get_hash(key) % line_count;
        
        ht_item_t *new_item = (ht_item_t *)malloc(sizeof(ht_item_t));
        if (new_item == NULL) {
            return;
        }

        // alokace a kopie klíče
        new_item->key = (char *)malloc(strlen(key) + 1);
        if (new_item->key == NULL) {
            free(new_item);
            return;
        }
        strcpy(new_item->key, key);
         
        // alokace a kopie hodnoty
        new_item->value = (char *)malloc(strlen(value) + 1);
        if (new_item->value == NULL) {
            free(new_item->key);
            free(new_item);
            return;
        }
        strcpy(new_item->value, value);

        // vložení na začátek seznamu
        new_item->next = (table)[index];
        (table)[index] = new_item;
    } else {
        // prvek existuje -> nahraď hodnotu
        char *new_value = (char *)malloc(strlen(value) + 1);
        if (new_value == NULL) {
            return; // necháme původní hodnotu
        }
        strcpy(new_value, value);

        free(item->value);
        item->value = new_value;
    }
}


  

/*
 * Získání hodnoty z tabulky.
 *
 * V případě úspěchu vrací funkce ukazatel na hodnotu prvku, v opačném
 * případě hodnotu NULL.
 *
 * Při implementaci využijte funkci ht_search.
 */
char *ht_get(ht_table_t table, char *key) {
  if(key==NULL || table == NULL)
    return NULL;
  ht_item_t *item = ht_search(table,key); // miesto v tabulke
  if (item == NULL ){
    return NULL;
  }
  else{
    return item->value;
  }
  
}

/*
 * Smazání prvku z tabulky.
 *
 * Funkce korektně uvolní všechny alokované zdroje přiřazené k danému prvku.
 * Pokud prvek neexistuje, funkce nedělá nic.
 *
 * Při implementaci NEPOUŽÍVEJTE funkci ht_search.
 */


/*
 * Smazání všech prvků z tabulky.
 *
 * Funkce korektně uvolní všechny alokované zdroje a uvede tabulku do stavu po 
 * inicializaci.
 */
void ht_delete_all(ht_table_t table) {
    if (table == NULL)
        return;
    
    for (int i = 0; i < line_count; i++) {
        //  printf("mažu index %d\n", i); 
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

