#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "dns_forwarder.h"
#include "dns_parser.h"
#include "hashtable.h"
#include "string_normalization.h"
#include <errno.h>
#include <sys/time.h>
#include <asm-generic/socket.h>
 #define PORT 53// Standard DNS port for resolver

// ADD THIS HELPER FUNCTION FOR FLAGS
void print_dns_header(HEADER *header) {
    uint16_t flags = ntohs(header->flags);
    
    printf("DNS Header:\n");
    printf("  ID: 0x%04x\n", ntohs(header->id));
    printf("  QR: %s\n", (flags & DNS_QR_MASK) ? "Response" : "Query");
    printf("  OPCODE: %d\n", (flags & DNS_OPCODE_MASK) >> 11);
    printf("  AA: %d\n", (flags & DNS_AA_MASK) ? 1 : 0);
    printf("  TC: %d\n", (flags & DNS_TC_MASK) ? 1 : 0);
    printf("  RD: %d\n", (flags & DNS_RD_MASK) ? 1 : 0);
    printf("  RA: %d\n", (flags & DNS_RA_MASK) ? 1 : 0);
    printf("  Z: %d\n", (flags & DNS_Z_MASK) >> 4);
    printf("  RCODE: %d\n", flags & DNS_RCODE_MASK);
    printf("  Questions: %d\n", (header->qdcount));
    printf("  Answers: %d\n", ntohs(header->ancount));
    printf("  Authorities: %d\n", ntohs(header->nscount));
    printf("  Additionals: %d\n", ntohs(header->arcount));
}

void send_receive(int sockfd, struct sockaddr_in *client_addr, 
                        socklen_t client_addr_len, unsigned char *buffer, 
                        size_t buffer_len, char *server_ip) {
    
    int sockfd_resolver = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd_resolver < 0) {
        perror("Error creating resolver socket");
        return;
    }

    // DEBUG: Print source port (important for response)
    struct sockaddr_in local_addr;
    socklen_t local_len = sizeof(local_addr);
    getsockname(sockfd_resolver, (struct sockaddr *)&local_addr, &local_len);
    printf("Using source port: %d\n", ntohs(local_addr.sin_port));

    // TIMEOUT - ABSOLÚTNE NEVYHNUTNÉ
    struct timeval timeout;
    timeout.tv_sec = 3;
    timeout.tv_usec = 0;
    if (setsockopt(sockfd_resolver, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("Error setting socket timeout");
        close(sockfd_resolver);
        return;
    }

    struct sockaddr_in resolver_addr;
    memset(&resolver_addr, 0, sizeof(resolver_addr));
    resolver_addr.sin_family = AF_INET;
    resolver_addr.sin_port = htons(PORT);
    
    if (inet_pton(AF_INET, server_ip, &resolver_addr.sin_addr) <= 0) {
        perror("Invalid resolver IP address");
        close(sockfd_resolver);
        return;
    }

    // DEBUG: Print what we're sending
    printf("Sending to resolver: %s:%d\n", server_ip, PORT);
    printf("DNS message ID: 0x%04x\n", ntohs(*(uint16_t*)buffer));

    // Send to resolver
    ssize_t sent = sendto(sockfd_resolver, buffer, buffer_len, 0,
                         (struct sockaddr *)&resolver_addr, sizeof(resolver_addr));
    if (sent < 0) {
        perror("Error sending to resolver");
        close(sockfd_resolver);
        return;
    }
    
    printf("Successfully sent %zd bytes to resolver\n", sent);

    // Receive response - CRITICAL: Must provide address structure
    struct sockaddr_in response_addr;
    socklen_t response_len = sizeof(response_addr);
    unsigned char response_buffer[65536];
    
    ssize_t received = recvfrom(sockfd_resolver, response_buffer, sizeof(response_buffer), 0,
                              (struct sockaddr *)&response_addr, &response_len);
    
    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            printf("Timeout: No response from resolver within 3 seconds\n");
        } else {
            perror("Error receiving from resolver");
        }
        close(sockfd_resolver);
        return;
    }
    
    // Verify response comes from expected resolver
    char response_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &response_addr.sin_addr, response_ip, INET_ADDRSTRLEN);
    
    printf("Received %zd bytes from %s:%d\n", received, response_ip, ntohs(response_addr.sin_port));
    printf("Response DNS ID: 0x%04x\n", ntohs(*(uint16_t*)response_buffer));

    close(sockfd_resolver);

    // Forward response back to client
    ssize_t sent_to_client = sendto(sockfd, response_buffer, received, 0, 
                                   (struct sockaddr *)client_addr, client_addr_len);
    
    if (sent_to_client < 0) {
        perror("Error sending response to client");
    } else {
        printf("Successfully forwarded %zd bytes to client\n", sent_to_client);
    }
}




// FIXED FUNCTION SIGNATURE - must match .h file!
int server_part(int port, ht_table_t table, char *server_ip_or_domain_name) {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int opt = 1;
    ssize_t receiving_msg;
    unsigned char buffer[1024] = { 0 };

    // Create a UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Error creating socket");
        return -1;
    }

    // Set socket options
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        close(sockfd);
        return -1;
    }

    // Set up the server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // Bind the socket to the specified port
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error binding socket");
        close(sockfd);
        return -1;
    }

    printf("UDP server listening on port %d\n", port);


        // Receive data from client
        receiving_msg = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0,
                                (struct sockaddr *)&client_addr, &client_addr_len);
        if (receiving_msg < 0) {
            perror("recvfrom");
            // continue;
        }

        printf("Received %zd bytes from client\n", receiving_msg);

        HEADER header;
        if(parse_dns_message(buffer, &header) < 0) {
            fprintf(stderr, "Failed to parse DNS message\n");
            // continue;
        }

        // USE THE HELPER FUNCTION instead of direct access
        print_dns_header(&header);

        if(ntohs(header.qdcount) > 0){
            size_t offset = sizeof(HEADER); // FIXED: Use sizeof, not hardcoded 12
            
            for(int i = 0; i < header.qdcount; i++) {
                dns_question_t question;
                if(parse_dns_question(buffer, receiving_msg, &question, &offset) < 0) {
                    fprintf(stderr, "Failed to parse DNS question\n");
                    break;
                }

                printf("DNS Question %d:\n", i+1);
                printf("  QNAME: %s\n", question.qname);
                printf("  QTYPE: %d\n", ntohs(question.flags.qtype));
                printf("  QCLASS: %d\n", ntohs(question.flags.qclass));

                char *main_domain = main_domain_finder(question.qname);
                int is_blocked = 0;
                
                if(main_domain != NULL && ht_search(table, main_domain) != NULL) {
                    printf("Domain %s is BLOCKED.\n", question.qname);
                    send_blocked_response(sockfd, &client_addr, client_addr_len, 
                                        buffer, receiving_msg, 5);
                    is_blocked = 1;
                } else {
                    printf("Domain %s is ALLOWED.\n", question.qname);
                    send_receive(sockfd, &client_addr, client_addr_len, 
                                 buffer, receiving_msg, server_ip_or_domain_name);
                }

                free(question.qname);
                free(main_domain);

                if (is_blocked) {
                    break;
                }
            }
        }
   

    close(sockfd);
    return 0;
}