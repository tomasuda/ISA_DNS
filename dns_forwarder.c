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
#include <pthread.h>
#include <signal.h>
#include <ctype.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <netdb.h> 

#define PORT 53
static volatile sig_atomic_t server_running = 1;

#define MAX_THREADS 50
sem_t *thread_sem ,*hash_lock;

// DNS header printer 
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
    printf("  Questions: %d\n", header->qdcount);
    printf("  Answers: %d\n", header->ancount);

}

// Cleanup function
void bye_termination(Arguments *args, ht_table_t table) {
    if (args) free(args);
    if (table) {
        ht_delete_all(table);
        table = NULL;
    }
    // cleanup semaphores + unmap if allocated
    if (thread_sem) {
        sem_destroy(thread_sem);
        munmap(thread_sem, sizeof(sem_t));
        thread_sem = NULL;
    }
    if (hash_lock) {
        sem_destroy(hash_lock);
        munmap(hash_lock, sizeof(sem_t));
        hash_lock = NULL;
    }
    exit(EXIT_SUCCESS);
}

// Signal handler
void signal_handler(int sig) {
   if(verbose_mode)printf("\nReceived signal %d, shutting down...\n", sig);
    server_running = 0;
}
void free_dns_question(dns_question_t *question) {
    if (question && question->qname) {
        free(question->qname);
        question->qname = NULL;
    }
}

//sending UDP mesages to resolver
//waits for response and resend them back to client
void send_receive(int sockfd, struct sockaddr_storage *client_addr, socklen_t client_addr_len,
                  unsigned char *buffer, size_t buffer_len, char *server_ip) {

    // Setup resolver address info
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; // IPv4 or IPv6
    hints.ai_socktype = SOCK_DGRAM;

    char portstr[6];
    snprintf(portstr, sizeof(portstr), "%d", PORT);

    if (getaddrinfo(server_ip, portstr, &hints, &res) != 0) {
        perror("getaddrinfo");
        return;
    }

    int resolver_sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (resolver_sock < 0) {
        perror("socket");
        freeaddrinfo(res);
        return;
    }

    // timeout 3s
    struct timeval tv = {3, 0};
    setsockopt(resolver_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    // send query
    if (sendto(resolver_sock, buffer, buffer_len, 0, res->ai_addr, res->ai_addrlen) < 0) {
        perror("sendto resolver");
        close(resolver_sock);
        freeaddrinfo(res);
        return;
    }

    // receive response
    unsigned char response[65536];
    struct sockaddr_storage from_addr;
    socklen_t from_len = sizeof(from_addr);
    ssize_t rlen = recvfrom(resolver_sock, response, sizeof(response), 0,
                            (struct sockaddr*)&from_addr, &from_len);
    if (rlen < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            if(verbose_mode) fprintf(stderr, "Timeout from resolver\n");
        } else {
            perror("recvfrom resolver");
        }
        close(resolver_sock);
        freeaddrinfo(res);
        return;
    }

    // forward response to client
    if (sendto(sockfd, response, rlen, 0, (struct sockaddr*)client_addr, client_addr_len) < 0) {
        perror("sendto client");
    }

    close(resolver_sock);
    freeaddrinfo(res);
}

// function for parsing dns mesages recieved in server
void *process_dns_message_thread(void *arg) {
    thread_data_t *data = (thread_data_t*)arg;
    if (!data) return NULL;

    if(verbose_mode) printf("Received %zd bytes from client\n", data->msg_len);

    HEADER header;
    //parsing datata and converting them to litle endian
    if(parse_dns_message(data->buffer, &header) < 0){
        fprintf(stderr,"Failed to parse DNS message\n");
        sem_post(thread_sem);
        free(data);
        return NULL;
    }

    if(verbose_mode) print_dns_header(&header);

    
    if(header.qdcount > 0){
        size_t offset = sizeof(HEADER);
        dns_question_t question;
        memset(&question,0,sizeof(question));
        if(parse_dns_question(data->buffer, data->msg_len, &question, &offset) < 0){
            fprintf(stderr,"Failed to parse DNS question\n");
            free_dns_question(&question);
            sem_post(thread_sem);
            free(data);
            return NULL;
        }

        
        char *domain_copy = strdup(question.qname);
        to_lower_str(domain_copy);
        char *current_domain;
        bool blocked = false;
        if(ht_search(data->table,".")==NULL){
            current_domain = domain_copy;
            do {
                if (ht_search(data->table, current_domain) != NULL) blocked = true;
                char *next_domain = domain_parser(current_domain); // now returns strdup'ed string
                if (current_domain != domain_copy) {
                    free(current_domain); 
                }
                current_domain = next_domain;
                
            } while(current_domain != NULL);
        } else {
            blocked = true;
        }
        printf("Domain requested: %s\n", question.qname);
        free(domain_copy);

        if(blocked){
            if(verbose_mode) printf("Domain %s blocked\n", question.qname);
            //sending responce to client 
            send_blocked_response(data->sockfd, (struct sockaddr*)&data->client_addr, data->client_addr_len,
                                  data->buffer, data->msg_len, 5);
        } else {
            // actual DNS resender logic
            send_receive(data->sockfd, &data->client_addr, data->client_addr_len,
                         data->buffer, data->msg_len, data->server_ip_or_domain_name);
        }

        free_dns_question(&question);
    }

    sem_post(thread_sem);
    free(data);
    return NULL;
}

//sets UDP sever on set port and listens on it  
int server_part(int port, ht_table_t table, char *server_ip_or_domain_name) {
    int sockfd;
    struct sockaddr_in6 server_addr6;
    struct sockaddr_storage client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    unsigned char buffer[4096];
    pthread_t tid;

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    sockfd = socket(AF_INET6, SOCK_DGRAM, 0);
    if(sockfd < 0){ perror("socket"); return -1;}

    int v6only = 0;
    setsockopt(sockfd, IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof(v6only));

    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // #ifdef SO_REUSEPORT
    //     setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
    // #endif

    memset(&server_addr6, 0, sizeof(server_addr6));
    server_addr6.sin6_family = AF_INET6;
    server_addr6.sin6_addr = in6addr_any;
    server_addr6.sin6_port = htons(port);

    if(bind(sockfd, (struct sockaddr*)&server_addr6, sizeof(server_addr6)) <0){
        perror("bind"); close(sockfd); return -1;
    }

    if(verbose_mode) printf("UDP server (IPv6 dual-stack) listening on port %d\n", port);

    // semaphore inicializatiom
    thread_sem = mmap(NULL,sizeof(sem_t),PROT_READ|PROT_WRITE,MAP_SHARED|MAP_ANONYMOUS,-1,0);
    hash_lock = mmap(NULL,sizeof(sem_t),PROT_READ|PROT_WRITE,MAP_SHARED|MAP_ANONYMOUS,-1,0);
    if(thread_sem == MAP_FAILED || hash_lock == MAP_FAILED){ perror("mmap"); close(sockfd); return -1;}
    sem_init(thread_sem,0,MAX_THREADS);
    sem_init(hash_lock,0,1);

    //main server loop
    while(server_running){
        struct timeval tv = {3,0};
        setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv));

        thread_data_t *data = malloc(sizeof(thread_data_t));
        if(!data) continue;

        ssize_t recieved_len = recvfrom(sockfd, buffer, sizeof(buffer)-1, 0,
                                (struct sockaddr*)&client_addr, &client_addr_len);
        if(recieved_len <0){
            if(errno==EAGAIN || errno==EWOULDBLOCK){ free(data); continue; }
            perror("recvfrom"); free(data); continue;
        }
        // storing data to struct for thread callback
        data->sockfd = sockfd;
        memcpy(&data->client_addr,&client_addr,sizeof(client_addr));
        data->client_addr_len = client_addr_len;
        data->msg_len = recieved_len;
        memcpy(data->buffer, buffer, recieved_len);
        data->table = table;
        data->server_ip_or_domain_name = server_ip_or_domain_name;

        sem_wait(thread_sem);
        if(pthread_create(&tid,NULL,process_dns_message_thread,data)==0){
            pthread_detach(tid);
        } else {
            perror("pthread_create"); free(data); sem_post(thread_sem);
        }
    }

    // cleanup
    if (thread_sem) {
        sem_destroy(thread_sem);
        munmap(thread_sem, sizeof(sem_t));
        thread_sem = NULL;
    }
    if (hash_lock) {
        sem_destroy(hash_lock);
        munmap(hash_lock, sizeof(sem_t));
        hash_lock = NULL;
    }
    close(sockfd);
    return 0;
}
