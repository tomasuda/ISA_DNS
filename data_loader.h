
/**
 * @file data_loader.h
 * @author Tomas Suda (xsudato00)
 * @date 20/9/2025
 * @brief loading and processing data
 */
#ifndef DATA_LOADER_H
#define DATA_LOADER_H

#include "hashtable.h"

extern int line_count;
int undesired_domains(char * file_name, ht_table_t table);
int count_lines(char *file_name);
#endif // DATA_LOADER_H