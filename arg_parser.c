/**
 * @file arg_parser.c
 * @author Tomas Suda (xsudato00)
 * @date 20/9/2025
 * @brief parsing arguments
 */
#include "arg_parser.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


void usage() {
    fprintf(stdout, "Usage: \n"); 
    fprintf(stdout, " MANDATORY:\n");
    fprintf(stdout, " -s  IP adresa nebo doménové jméno DNS serveru (resolveru), kam se má zaslat dotaz\n");
    fprintf(stdout, " -f filter_file: Jméno souboru obsahující nežádoucí domény\n");
    fprintf(stdout, " OPTIONAL:\n");
    fprintf(stdout, " -p port: Číslo portu, na kterém bude program očekávat dotazy. Výchozí je port 53\n");
    fprintf(stdout, " -h (help)\n");
}

int parser(int argc, char *argv[], Arguments *chat_server_args) {
    char  *target_address = NULL ,*src_file_name = NULL;
    int help_flag = 0, opt;
    int port = 53;

    while ((opt = getopt(argc, argv, "s:p:f:")) != -1) {
        switch (opt) {
            case 's': target_address = optarg; break;
            case 'p':
            {
                for (char *p = optarg; *p != '\0'; p++) {
                    if (!isdigit((unsigned char)*p)) {
                        usage();
                        exit(EXIT_FAILURE);
                    }
                }
                port = atoi(optarg);
                break;
            }
            case 'f': src_file_name = optarg; break;
            case 'h': help_flag = 1; break;
            default: usage(); exit(EXIT_FAILURE);
        }
    }
    if (help_flag) {
        usage();
        exit(EXIT_SUCCESS);
    }
    if (!target_address) {
        fprintf(stdout, "Error: Missing -s argument.\n");
        usage();
        return -1;
    }
    if (!src_file_name) {
        fprintf(stdout, "Error: Missing -f argument.\n");
        usage();
        return -1;
    }
    if(port < 1 || port > 65535) {
        fprintf(stdout, "Error: Invalid port number. Must be between 1 and 65535.\n");
        usage();
        return -1;
    }
   
    
   
   
    strcpy(chat_server_args->server_ip_or_domain_name, target_address);
    strcpy(chat_server_args->file_name, src_file_name);
    chat_server_args->server_port = port;
    return 1;
}
