#ifndef DNS_PARSER_H
#define DNS_PARSER_H

#include <stddef.h>
#include <stdint.h>
#include <arpa/inet.h>
#include "dns_forwarder.h"



int parse_dns_message(const unsigned char *buffer, HEADER *header);
int parse_dns_question(const unsigned char *buffer, size_t buffer_len, 
                      dns_question_t *question, size_t *offset);

// int repackage_dns_message(dns_message_t *message, unsigned char **buffer, size_t *buffer_len);
void send_blocked_response(int sockfd, struct sockaddr *client_addr, socklen_t client_addr_len,
    const unsigned char *request_buffer, size_t request_len, int rcode);

#endif // DNS_PARSER_H