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
#include <stdbool.h>
#include "main.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

void usage() {
    fprintf(stdout, "Usage: \n"); 
    fprintf(stdout, "dns_forwarder -s server_ip_or_domain_name -f filter_file [-p port] [-v] [-h]\n");
    fprintf(stdout, "DONT USE -v WITH TO MANY INPUTING PACKETS IT WILL RESULT IN RACE CONDITIONS\n");
    fprintf(stdout, " MANDATORY:\n");
    fprintf(stdout, " -s  IP adresa nebo doménové jméno DNS serveru (resolveru), kam se má zaslat dotaz\n");
    fprintf(stdout, " -f filter_file: Jméno souboru obsahující nežádoucí domény\n");
    fprintf(stdout, " OPTIONAL:\n");
    fprintf(stdout, " -p port: Číslo portu, na kterém bude program očekávat dotazy. Výchozí je port 53\n");
    fprintf(stdout, " -v (verbose) - zapne režim s podrobným výstupem\n");
    fprintf(stdout, " -h (help)\n");

}

int parser(int argc, char *argv[], Arguments *chat_server_args) {
    char *target_address = NULL, *src_file_name = NULL;
    int help_flag = 0, opt;
    int port = 53;
    bool used_s = false, used_p = false, used_f = false, used_v = false;
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_UNSPEC;     
    hints.ai_socktype = SOCK_DGRAM; 
    hints.ai_flags = AI_NUMERICSERV;
    
    if (argc == 1) {
        usage();
        exit(EXIT_SUCCESS);
    }

    while ((opt = getopt(argc, argv, "s:p:f:vh")) != -1) {
        switch (opt) {
            case 's': {
                if (used_s) {
                    fprintf(stderr, "ERROR: cant have duplicit arguments\n");
                    usage();
                    exit(EXIT_FAILURE);
                }
                if (optarg == NULL || optarg[0] == '-') {
                    fprintf(stderr, "ERROR: -s requires argument\n");
                    usage();
                    exit(EXIT_FAILURE);
                }
                if (getaddrinfo(optarg, NULL, &hints, &res) != 0) {
                    fprintf(stderr,"invalid IP ar domainname/n");
                    usage();
                    exit(EXIT_FAILURE);
                }
                target_address = optarg; 
                used_s = true;
                break;
            }
            case 'p': {
                if (used_p) {
                    fprintf(stderr, "ERROR: cant have duplicit arguments\n");
                    usage();
                    exit(EXIT_FAILURE);
                }
                if (optarg == NULL || optarg[0] == '-') {
                    fprintf(stderr, "ERROR: -p requires argument\n");
                    usage();
                    exit(EXIT_FAILURE);
                }
                for (char *p = optarg; *p != '\0'; p++) {
                    if (!isdigit((unsigned char)*p)) {
                        fprintf(stderr, "ERROR: value of port must be number\n");
                        usage();
                        exit(EXIT_FAILURE);
                    }
                }
                port = atoi(optarg);
                used_p = true;
                break;
            }
            case 'f': { 
                if (used_f) {
                    fprintf(stderr, "ERROR: cant have duplicit arguments\n");
                    usage();
                    exit(EXIT_FAILURE);
                }
                if (optarg == NULL || optarg[0] == '-') {
                    fprintf(stderr, "ERROR: -f requires argument\n");
                    usage();
                    exit(EXIT_FAILURE);
                }
                src_file_name = optarg;
                used_f = true;
                break;
            }
            case 'v': {
                if (used_v) {
                    fprintf(stderr, "ERROR: cant have duplicit arguments\n");
                    usage();
                    exit(EXIT_FAILURE);
                }
                verbose_mode = 1;
                used_v = true;
                break;
            }
            case 'h': 
                help_flag = 1; 
                break;
                                         
            default: 
                fprintf(stderr, "Wrong argument\n"); 
                usage();  
                exit(EXIT_FAILURE);
        }
    }

    // Zvyšok kódu zostáva rovnaký
    if (help_flag) {
        if (used_s || used_f || used_p || used_v) {
            fprintf(stderr, "ERROR: cant combine help with other arguments\n");
            usage();
            exit(EXIT_FAILURE);
        }
        usage();
        exit(EXIT_SUCCESS);
    }
    
    if (target_address==NULL) {
        fprintf(stderr, "Error: Missing -s argument.\n");
        usage();
        return -1;
    }
    
    if (!src_file_name) {
        fprintf(stderr, "Error: Missing -f argument.\n");
        usage();
        return -1;
    }
    
    if (port < 1 || port > 65535) {
        fprintf(stderr, "Error: Invalid port number. Must be between 1 and 65535.\n");
        usage();
        return -1;
    }
   
    strcpy(chat_server_args->server_ip_or_domain_name, target_address);
    strcpy(chat_server_args->file_name, src_file_name);
    chat_server_args->server_port = port;
    freeaddrinfo(res);
    return 1;
}