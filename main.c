/**
 * @file main.c
 * @author Tomas Suda (xsudato00)
 * @date 20/9/2025
 * @brief starting point of the program
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "arg_parser.h"
#include "data_loader.h"
#include "main.h"
#include "hashtable.h"
#include "dns_forwarder.h"
#include <signal.h>


volatile sig_atomic_t stop_flag = 0;
Arguments *scener_args = NULL;
ht_table_t table = NULL;


void handle_sigint() {
    stop_flag = 1;
        bye_termination(scener_args, table);

}

void setup_signal_handlers() {
    struct sigaction stdin_signals;
    stdin_signals.sa_handler = handle_sigint;
    sigemptyset(&stdin_signals.sa_mask);
    stdin_signals.sa_flags = 0;
    sigaction(SIGINT, &stdin_signals, NULL);
}



int main(int argc, char *argv[]) {
    setup_signal_handlers();
 


    scener_args =(Arguments*) malloc(sizeof(Arguments));
    if (scener_args == NULL) {
        fprintf(stderr, "Error: Failed to allocate memory for arguments.\n");
        return EXIT_FAILURE;
    }
    memset(scener_args, 0, sizeof(Arguments));
    if (parser(argc, argv, scener_args)<0) {
        fprintf(stderr, "Error: Failed to parse arguments.\n");
        free(scener_args);
        return EXIT_FAILURE;
    }
    printf("Server: %s\n", scener_args->server_ip_or_domain_name);
    printf("Port: %d\n", scener_args->server_port);
    printf("File: %s\n", scener_args->file_name);
    

    if (count_lines(scener_args->file_name) < 0) {
        fprintf(stderr, "Error: Failed to process file '%s'.\n", scener_args->file_name);
        free(scener_args);
        return EXIT_FAILURE;
    }
    table = malloc(sizeof(ht_item_t*) * line_count);
    if (table == NULL) {
        fprintf(stderr, "Error: Failed to allocate memory for hash table.\n");
        free(scener_args);
        return EXIT_FAILURE;
    }
    ht_init(table);

       if (undesired_domains(scener_args->file_name, table) < 0) {

           fprintf(stderr, "Error: Failed to process file '%s'.\n", scener_args->file_name);
           free(scener_args);
           return EXIT_FAILURE;
       }

    // Start the server part
    int server_socket = server_part(scener_args->server_port, table, scener_args->server_ip_or_domain_name);
    if (server_socket < 0) {
        fprintf(stderr, "Error: Failed to start server on port %d.\n", scener_args->server_port);
        free(scener_args);
        ht_delete_all(table);
        return EXIT_FAILURE;
    }
    free(scener_args);
    ht_delete_all(table);
    // exit(42); // Placeholder exit code, replace with actual logic   
  
    return EXIT_SUCCESS;
}