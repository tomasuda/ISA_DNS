
/**
 * @file dns_parser.c
 * @author Tomas Suda (xsudato00)
 * @date 20/9/2025
 */
#include "dns_forwarder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dns_parser.h"
#include <arpa/inet.h>

//functin for converting network byte order to host byte order
int parse_dns_message(const unsigned char *buffer, HEADER *header)
{
    if (!buffer || !header) return -1;

    memcpy(header, buffer, 12);
    header->id = ntohs(header->id);
    header->qdcount = ntohs(header->qdcount);
    header->ancount = ntohs(header->ancount);
    header->nscount = ntohs(header->nscount);
    header->arcount = ntohs(header->arcount);
    return 0;
}

// Function to decode a DNS name, handling compression
char *decode_dns_name(const unsigned char *buffer, size_t buffer_len, 
                     size_t *offset) {
    if (*offset >= buffer_len) return NULL;
    
    size_t pos = *offset;
    size_t name_len = 0;
    size_t compressed = 0;
    
    // First pass: calculate length
    while (pos < buffer_len && buffer[pos] != 0) {
        if ((buffer[pos] & 0xC0) == 0xC0) { // Compression pointer
            if (pos + 1 >= buffer_len) return NULL;
            size_t pointer = ((buffer[pos] & 0x3F) << 8) | buffer[pos + 1];
            if (pointer >= buffer_len) return NULL;
            
            if (!compressed) {
                compressed = pos + 2;
            }
            pos = pointer;
            continue;
        }
        
        if (buffer[pos] > 63) return NULL; // Invalid length
        name_len += buffer[pos] + 1; // +1 for dot or null
        pos += buffer[pos] + 1;
        
        if (pos >= buffer_len) return NULL;
    }
    
    if (!compressed) {
        name_len++; // For null terminator
    }
    
    // Allocate memory for name
    char *name = malloc(name_len);
    if (!name) return NULL;
    
   //build the name
    pos = *offset;
    size_t name_pos = 0;
    int first = 1;
    
    while (pos < buffer_len && buffer[pos] != 0) {
        if ((buffer[pos] & 0xC0) == 0xC0) { // Compression pointer
            size_t pointer = ((buffer[pos] & 0x3F) << 8) | buffer[pos + 1];
            if (pointer >= buffer_len) {
                free(name);
                return NULL;
            }
            pos = pointer;
            continue;
        }
        
        uint8_t label_len = buffer[pos++];
        if (label_len > 63 || pos + label_len > buffer_len) {
            free(name);
            return NULL;
        }
        
        if (!first) {
            name[name_pos++] = '.';
        }
        first = 0;
        
        memcpy(name + name_pos, buffer + pos, label_len);
        name_pos += label_len;
        pos += label_len;
    }
    
    name[name_pos] = '\0';
    
    if (!compressed) {
        *offset = pos + 1; // Skip the null byte
    } else {
        *offset = compressed;
    }
    
    return name;
}

int parse_dns_question(const unsigned char *buffer, size_t buffer_len, 
                      dns_question_t *question, size_t *offset) {
    if (!buffer || !question || !offset || *offset >= buffer_len) return -1;
    
    // Parse QNAME
    question->qname = decode_dns_name(buffer, buffer_len, offset);
    if (!question->qname) return -1;
    
    // Parse QTYPE and QCLASS
    if (*offset + 4 > buffer_len) {
        free(question->qname);
        return -1;
    }
    
    // Copy the Q_FLAGS
    memcpy(&question->flags, buffer + *offset, 4);
    
    // Convert network byte order to host byte order
    question->flags.qtype = ntohs(question->flags.qtype);
    question->flags.qclass = ntohs(question->flags.qclass);
    
    *offset += 4;
    
    return 0;
}

size_t calculate_dns_message_length(const unsigned char *dns_msg, size_t max_len) {
    if (max_len < sizeof(HEADER)) return 0;
    
    HEADER *header = (HEADER *)dns_msg;
    size_t offset = sizeof(HEADER);
    
    // Skip questions
    for (int i = 0; i < header->qdcount && offset < max_len; i++) {
        // Skip QNAME (domain name)
        while (offset < max_len && dns_msg[offset] != 0) {
            offset += dns_msg[offset] + 1;
        }
        if (offset >= max_len) break;
        offset++; // Skip null byte
        
        // Skip QTYPE and QCLASS
        offset += 4;
    }
    
    return offset; // Length up to end of questions
}

void send_blocked_response(int sockfd, struct sockaddr *client_addr, socklen_t client_addr_len,
    const unsigned char *request_buffer, size_t request_len, int rcode) {

    // Calculate message length up to end of questions
    size_t message_length = calculate_dns_message_length(request_buffer, request_len);
 
    unsigned char response[1024];
    memcpy(response, request_buffer, message_length);

    // DNS header
    HEADER *response_header = (HEADER *)response;

    // Convert flags to host byte order
    uint16_t flags = ntohs(response_header->flags);

    flags |= DNS_QR_MASK;   // Set QR = 1 (Response)
    flags &= ~DNS_RCODE_MASK; // Clear RCODE bits
    flags |= rcode;        
    if (flags & DNS_RD_MASK) {
        flags |= DNS_RA_MASK; // If RD=1, set RA=1
    }
    // Convert flags back to network byte order
    response_header->flags = htons(flags);
    // Send response
    ssize_t sent_bytes = sendto(sockfd, response, message_length, 0,(struct sockaddr *)client_addr, client_addr_len);
    if (sent_bytes < 0) {
        perror("sendto");
    } else if (verbose_mode) {
        printf("Sent blocked response (%zd bytes) to client\n", sent_bytes);
    }

}