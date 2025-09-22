/**
 * @file main.h
 * @author Tomas Suda (xsudato00)
 * @date 20/9/2025
 */
#ifndef MAIN_H
#define MAIN_H


#define PROTOCOL_STRING_LENGTH 4
#define MAX_IP_HOSTNAME_LEN 1024
#define MAX_FILE_NAME_LEN 256

extern int verbose_mode;
typedef struct {
    char server_ip_or_domain_name[MAX_IP_HOSTNAME_LEN];
    char file_name[MAX_FILE_NAME_LEN];
    int server_port;
} Arguments;


#endif // MAIN_H