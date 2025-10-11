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
#include "main.h"
#include <sys/time.h>
#include <asm-generic/socket.h>
#include <pthread.h>
#include <signal.h>
#include <ctype.h>
#include <semaphore.h>
#include <sys/mman.h>

#define PORT 53 // Standard DNS port for resolver
static volatile sig_atomic_t server_running = 1;

#define MAX_THREADS 50
sem_t *thread_sem ,*hash_lock;

    

// ADD THIS HELPER FUNCTION FOR FLAGS
void print_dns_header(HEADER *header) {
    uint16_t flags = ntohs(header->flags);
    
    printf("DNS Header:\n");
    printf("  ID: 0x%04x\n", header->id);
    printf("  QR: %s\n", (flags & DNS_QR_MASK) ? "Response" : "Query");
    printf("  OPCODE: %d\n", (flags & DNS_OPCODE_MASK) >> 11);
    printf("  AA: %d\n", (flags & DNS_AA_MASK) ? 1 : 0);
    printf("  TC: %d\n", (flags & DNS_TC_MASK) ? 1 : 0);
    printf("  RD: %d\n", (flags & DNS_RD_MASK) ? 1 : 0);
    printf("  RA: %d\n", (flags & DNS_RA_MASK) ? 1 : 0);
    printf("  Z: %d\n", (flags & DNS_Z_MASK) >> 4);
    printf("  RCODE: %d\n", flags & DNS_RCODE_MASK);
    printf("  Questions: %d\n", header->qdcount); // FIXED: Added ntohs()
    printf("  Answers: %d\n", header->ancount);
    printf("  Authorities: %d\n", header->nscount);  
    printf("  Additional: %d\n", header->arcount);
}

void bye_termination(Arguments *args, ht_table_t table) { // FIXED: pointer to table
    if (args) {
            free(args);
    }   
    if(table && table){
        ht_delete_all(table);
        table = NULL;
    }
    exit(EXIT_SUCCESS);
}

void signal_handler(int sig) {
   if(verbose_mode){
       printf("\nReceived signal %d, shutting down...\n", sig);
   }
    server_running = 0;
}

void send_receive(int sockfd, struct sockaddr_in *client_addr, socklen_t client_addr_len, unsigned char *buffer,
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

    // Send to resolver
    ssize_t sent = sendto(sockfd_resolver, buffer, buffer_len, 0,
                         (struct sockaddr *)&resolver_addr, sizeof(resolver_addr));
    if (sent < 0) {
        perror("Error sending to resolver");
        close(sockfd_resolver);
        return;
    }

    // Receive response - CRITICAL: Must provide address structure
    struct sockaddr_in response_addr;
    socklen_t response_len = sizeof(response_addr);
    unsigned char response_buffer[65536];
    
    ssize_t received = recvfrom(sockfd_resolver, response_buffer, sizeof(response_buffer), 0,
                              (struct sockaddr *)&response_addr, &response_len);
    
    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // if(verbose_mode){
                fprintf(stderr, "Timeout: No response from resolver within 1 secondff\n");
                
            // }
        } else {
            perror("Error receiving from resolver");
        }
        close(sockfd_resolver);
        return;
    }
    
    // Verify response comes from expected resolver
    char response_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &response_addr.sin_addr, response_ip, INET_ADDRSTRLEN);

    close(sockfd_resolver);

    // Forward response back to client
    ssize_t sent_to_client = sendto(sockfd, response_buffer, received, 0, 
                                   (struct sockaddr *)client_addr, client_addr_len);
    
    if (sent_to_client < 0) {
        perror("Error sending response to client");
    } 
}

// FIXED: Added function to free DNS question resources
void free_dns_question(dns_question_t *question) {
    if (question && question->qname) {
        free(question->qname);
        question->qname = NULL;
    }
}

void *process_dns_message_thread(void *arg) {

       thread_data_t *data = (thread_data_t *)arg;
    
    // DIAGNOSTIKA
    int sem_value;
    sem_getvalue(thread_sem, &sem_value);
    fprintf(stderr, "🚀 [THREAD %ld] Spúšťam, semafor: %d/%d, buffer: %zd bytes\n", 
           pthread_self(), MAX_THREADS - sem_value, MAX_THREADS, data->msg_len);
    fflush(stderr);

    //bool is_blocked = false;

    if(verbose_mode) printf("Received %zd bytes from client\n", data->msg_len);

    HEADER header;
    if(parse_dns_message(data->buffer, &header) < 0) {
        fprintf(stderr, "❌ [THREAD %ld] Failed to parse DNS message\n", pthread_self());
        sem_post(thread_sem);
        free(data);
        return NULL;
    }
    
    // DIAGNOSTIKA hlavičky
    fprintf(stderr, "📋 [THREAD %ld] DNS ID: 0x%04x, Questions: %d\n", 
           pthread_self(), (header.id), (header.qdcount));
    fflush(stderr);
   // thread_data_t *data = (thread_data_t *)arg;
    bool is_blocked = false;

    if(verbose_mode) fprintf(stderr, "Received %zd bytes from client\n", data->msg_len);

    //HEADER header;
    if(parse_dns_message(data->buffer, &header) < 0) {
        fprintf(stderr, "Failed to parse DNS message\n");
        sem_post(thread_sem);
        free(data);
        return NULL;
    }
    
    if(verbose_mode) print_dns_header(&header);

    if(header.qdcount > 0){
        size_t offset = sizeof(HEADER);
       
        for(int i = 0; i < header.qdcount; i++) {
            dns_question_t question;
            memset(&question, 0, sizeof(question));
            int parse_result = parse_dns_question(data->buffer, data->msg_len, &question, &offset);
            
            if(parse_result < 0) {
                fprintf(stderr, "Failed to parse DNS question %d\n", parse_result);
                sem_post(thread_sem);
                free(data);
                return NULL;
            }
            
            if(verbose_mode){
                printf("DNS Question %d:\n", i+1);
                printf("  QNAME: %s\n", question.qname);
                printf("  QTYPE: %d\n", question.flags.qtype);
                printf("  QCLASS: %d\n", question.flags.qclass);
            }

            // BEZPEČNÁ verzia s kópiou reťazca
            char *domain_copy = strdup(question.qname);
            if (!domain_copy) {
                free_dns_question(&question);
                sem_post(thread_sem);
                free(data);
                return NULL;
            }
            to_lower_str(domain_copy);
            
            char *sub_domain = domain_copy;
            char *prev_domain = NULL;
            is_blocked = false;

            while (sub_domain != NULL) {
                // Skontrolujte blokovanie
   
                if (ht_search(data->table, ".") != NULL || (sub_domain != NULL && ht_search(data->table, sub_domain) != NULL)) {
                    is_blocked = true;
                    break;
                }
                
                // Získajte ďalšiu úroveň domény
                prev_domain = sub_domain;
                sub_domain = domain_parser(sub_domain);
                
                // Ukončite ak sa doména nemení alebo je NULL
                if (sub_domain == NULL || strcmp(sub_domain, prev_domain) == 0) {
                    break;
                }
            }
            
            free(domain_copy);

            if (is_blocked){
                if(verbose_mode) printf("Domain %s is BLOCKED.\n", question.qname);
                send_blocked_response(data->sockfd, &data->client_addr, data->client_addr_len, 
                                    data->buffer, data->msg_len, 5);
                free_dns_question(&question);
                break;
            }
            else{
                if(verbose_mode) printf("Domain %s is ALLOWED.\n", question.qname);
                // Spracujte ďalšie otázky alebo pošlite dopredu
            }

            free_dns_question(&question);
        }
        
        // Ak nebolo blokované, pošlite dopredu všetky otázky naraz
        if (!is_blocked) {
            send_receive(data->sockfd, &data->client_addr, data->client_addr_len,
                        data->buffer, data->msg_len, data->server_ip_or_domain_name);
        }
    }
    
    sem_post(thread_sem);
    free(data);
    return NULL;
}

int server_part(int port, ht_table_t table, char *server_ip_or_domain_name) {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int opt = 1;
    ssize_t receiving_msg;
    pthread_t tid;
    unsigned char buffer[4096] = { 0 };

    // Register signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

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
    if(verbose_mode) printf("UDP server listening on port %d\n", port);
    // Upravená slučka s podmienkou pre graceful shutdown
     thread_sem = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
     hash_lock = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (sem_init(thread_sem, 0, MAX_THREADS) != 0) {
        perror("sem_init");
        close(sockfd);
        return -1;
    }
    if (sem_init(hash_lock, 0, 1) != 0) {
        perror("sem_init");
        close(sockfd);
        return -1;
    }

    while (server_running) {
        // Nastavíme timeout na recvfrom, aby sme mohli pravidelne kontrolovať server_running
        struct timeval tv;
        tv.tv_sec = 3;
        tv.tv_usec = 0;
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        thread_data_t *data = malloc(sizeof(thread_data_t));
        if (!data) {
            perror("malloc");
            continue;
        }

        sem_wait(thread_sem);
        receiving_msg = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0,
                                (struct sockaddr *)&client_addr, &client_addr_len);

        
        
        if (receiving_msg < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Timeout - skontrolujeme, či máme pokračovať
                free(data);
                continue;
            } else {
                perror("recvfrom");
                free(data);
                continue;
            }
        }

        //Fill thread data structure
        data->sockfd = sockfd;
        data->client_addr = client_addr;
        data->client_addr_len = client_addr_len;
        data->msg_len = receiving_msg;
        memcpy(data->buffer, buffer, receiving_msg);
        data->table = table;
        data->server_ip_or_domain_name = server_ip_or_domain_name;

        if (pthread_create(&tid, NULL, process_dns_message_thread, data) == 0) {
            pthread_detach(tid);
        } else {
            perror("pthread_create");
            free(data);
        }
    }
    sem_destroy(thread_sem);
    close(sockfd);
    
    return 0;
}