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
#include <netdb.h>

#define PORT 53 // Standard DNS port for resolver
static volatile sig_atomic_t server_running = 1;

#define MAX_THREADS 50
sem_t *thread_sem ,*hash_lock;

// Helper: print DNS header (assumes header fields are already in host byte order)
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
    printf("  Authorities: %d\n", header->nscount);
    printf("  Additional: %d\n", header->arcount);
}

void bye_termination(Arguments *args, ht_table_t table) {
    if (args) free(args);
    if (table) {
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

// send_receive: supports IPv4/IPv6 upstream resolver (server_ip can be literal IP or hostname)
void send_receive(int sockfd, struct sockaddr_storage *client_addr, socklen_t client_addr_len, unsigned char *buffer,
                  size_t buffer_len, char *server_ip) {
    struct addrinfo hints, *res = NULL, *rp = NULL;
    int sockfd_resolver = -1;
    char portstr[6];
    snprintf(portstr, sizeof(portstr), "%d", PORT);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; // IPv4 or IPv6
    hints.ai_socktype = SOCK_DGRAM;

    int gai = getaddrinfo(server_ip, portstr, &hints, &res);
    if (gai != 0) {
        fprintf(stderr, "getaddrinfo(%s): %s\n", server_ip, gai_strerror(gai));
        return;
    }

    // try addresses until one works
    for (rp = res; rp != NULL; rp = rp->ai_next) {
        sockfd_resolver = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockfd_resolver < 0) continue;

        // set recv timeout
        struct timeval timeout = { .tv_sec = 3, .tv_usec = 0 };
        if (setsockopt(sockfd_resolver, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
            perror("setsockopt SO_RCVTIMEO");
            close(sockfd_resolver);
            sockfd_resolver = -1;
            continue;
        }

        // send
        ssize_t sent = sendto(sockfd_resolver, buffer, buffer_len, 0, rp->ai_addr, rp->ai_addrlen);
        if (sent < 0) {
            perror("Error sending to resolver");
            close(sockfd_resolver);
            sockfd_resolver = -1;
            continue;
        }

        // got socket and sent, break to receive
        break;
    }

    if (sockfd_resolver < 0) {
        fprintf(stderr, "Failed to contact resolver %s\n", server_ip);
        freeaddrinfo(res);
        return;
    }

    // receive
    unsigned char response_buffer[65536];
    struct sockaddr_storage response_addr;
    socklen_t response_len = sizeof(response_addr);
    ssize_t received = recvfrom(sockfd_resolver, response_buffer, sizeof(response_buffer), 0,
                                (struct sockaddr *)&response_addr, &response_len);
    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            if (verbose_mode) fprintf(stderr, "Timeout waiting for resolver %s\n", server_ip);
        } else {
            perror("recvfrom from resolver");
        }
        close(sockfd_resolver);
        freeaddrinfo(res);
        return;
    }

    // optional: log resolver address
    char ipbuf[INET6_ADDRSTRLEN];
    if (response_addr.ss_family == AF_INET) {
        struct sockaddr_in *s = (struct sockaddr_in *)&response_addr;
        inet_ntop(AF_INET, &s->sin_addr, ipbuf, sizeof(ipbuf));
    } else if (response_addr.ss_family == AF_INET6) {
        struct sockaddr_in6 *s6 = (struct sockaddr_in6 *)&response_addr;
        inet_ntop(AF_INET6, &s6->sin6_addr, ipbuf, sizeof(ipbuf));
    } else {
        strncpy(ipbuf, "unknown", sizeof(ipbuf));
        ipbuf[sizeof(ipbuf)-1] = '\0';
    }
    if (verbose_mode) fprintf(stderr, "Resolver replied from %s\n", ipbuf);
    char ip_str[INET6_ADDRSTRLEN];
    
if(client_addr->ss_family == AF_INET6) {
    inet_ntop(AF_INET6, &((struct sockaddr_in6 *)client_addr)->sin6_addr, ip_str, sizeof(ip_str));
} else {
    inet_ntop(AF_INET, &((struct sockaddr_in *)client_addr)->sin_addr, ip_str, sizeof(ip_str));
}
printf("Sending response to %s\n", ip_str);


    // forward back to client (client_addr may be IPv4 or IPv6)
    ssize_t sent_to_client = sendto(sockfd, response_buffer, received, 0,
                                   (struct sockaddr *)client_addr, client_addr_len);
    if (sent_to_client < 0) perror("Error sending response to client");

    close(sockfd_resolver);
    freeaddrinfo(res);
}

void free_dns_question(dns_question_t *question) {
    if (question && question->qname) {
        free(question->qname);
        question->qname = NULL;
    }
}

void *process_dns_message_thread(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;
    if (!data) return NULL;

    int sem_value;
    sem_getvalue(thread_sem, &sem_value);
    fprintf(stderr, "[THREAD %lu] start, sem: %d/%d, buffer: %zd bytes\n",
            (unsigned long)pthread_self(), MAX_THREADS - sem_value, MAX_THREADS, data->msg_len);

    if (verbose_mode) printf("Received %zd bytes from client\n", data->msg_len);

    HEADER header;
    if (parse_dns_message(data->buffer, &header) < 0) {
        fprintf(stderr, "Failed to parse DNS message\n");
        sem_post(thread_sem);
        free(data);
        return NULL;
    }

    if (verbose_mode) print_dns_header(&header);

    bool is_blocked = false;

    if (header.qdcount > 0) {
        size_t offset = sizeof(HEADER);
        for (int i = 0; i < header.qdcount; i++) {
            dns_question_t question;
            memset(&question, 0, sizeof(question));
            int parse_result = parse_dns_question(data->buffer, data->msg_len, &question, &offset);
            if (parse_result < 0) {
                fprintf(stderr, "Failed to parse DNS question %d\n", parse_result);
                free_dns_question(&question);
                break;
            }

            if (verbose_mode) {
                printf("DNS Question %d:\n  QNAME: %s\n  QTYPE: %d\n  QCLASS: %d\n",
                       i+1, question.qname, question.flags.qtype, question.flags.qclass);
            }

            char *domain_copy = strdup(question.qname);
            if (!domain_copy) {
                free_dns_question(&question);
                break;
            }
            to_lower_str(domain_copy);

            char *sub_domain = domain_copy;
            char *prev_domain = NULL;
            is_blocked = false;
            while (sub_domain != NULL) {
                if (ht_search(data->table, ".") != NULL || (sub_domain != NULL && ht_search(data->table, sub_domain) != NULL)) {
                    is_blocked = true;
                    break;
                }
                prev_domain = sub_domain;
                sub_domain = domain_parser(sub_domain);
                if (sub_domain == NULL || strcmp(sub_domain, prev_domain) == 0) break;
            }

            free(domain_copy);

            if (is_blocked) {
                if (verbose_mode) printf("Domain %s is BLOCKED.\n", question.qname);
                // send_blocked_response signature expects struct sockaddr*
                send_blocked_response(data->sockfd, (struct sockaddr *)&data->client_addr, data->client_addr_len,
                                      data->buffer, data->msg_len, 5);
                free_dns_question(&question);
                break; // do not forward
            } else {
                if (verbose_mode) printf("Domain %s is ALLOWED.\n", question.qname);
            }

            free_dns_question(&question);
        }

        if (!is_blocked) {
            // forward upstream
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
    struct sockaddr_in6 server_addr6;
    struct sockaddr_storage client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int opt = 1;
    ssize_t receiving_msg;
    pthread_t tid;
    unsigned char buffer[4096];

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // create IPv6 socket and allow dual-stack
    sockfd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (sockfd < 0) { perror("socket"); return -1; }

    int v6only = 0;
    if (setsockopt(sockfd, IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof(v6only)) < 0) {
        if (verbose_mode) perror("setsockopt IPV6_V6ONLY");
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt SO_REUSEADDR"); close(sockfd); return -1;
    }
#ifdef SO_REUSEPORT
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
        if (verbose_mode) perror("setsockopt SO_REUSEPORT");
    }
#endif

    memset(&server_addr6, 0, sizeof(server_addr6));
    server_addr6.sin6_family = AF_INET6;
    server_addr6.sin6_addr = in6addr_any;
    server_addr6.sin6_port = htons(port);

    if (bind(sockfd, (struct sockaddr *)&server_addr6, sizeof(server_addr6)) < 0) {
        perror("bind"); close(sockfd); return -1;
    }

    if (verbose_mode) printf("UDP server (IPv6 dual-stack) listening on port %d\n", port);

    thread_sem = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    hash_lock = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (thread_sem == MAP_FAILED || hash_lock == MAP_FAILED) { perror("mmap"); close(sockfd); return -1; }

    if (sem_init(thread_sem, 0, MAX_THREADS) != 0) { perror("sem_init thread_sem"); close(sockfd); return -1; }
    if (sem_init(hash_lock, 0, 1) != 0) { perror("sem_init hash_lock"); close(sockfd); return -1; }

    while (server_running) {
        struct timeval tv = { .tv_sec = 3, .tv_usec = 0 };
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        thread_data_t *data = malloc(sizeof(thread_data_t));
        if (!data) { perror("malloc"); continue; }

        sem_wait(thread_sem);
        receiving_msg = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0,
                                 (struct sockaddr *)&client_addr, &client_addr_len);

        if (receiving_msg < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) { free(data); continue; }
            perror("recvfrom"); free(data); continue;
        }

        // fill data; ensure thread_data_t has compatible fields
        data->sockfd = sockfd;
        memcpy(&data->client_addr, &client_addr, sizeof(client_addr));
        data->client_addr_len = client_addr_len;
        data->msg_len = receiving_msg;
        memcpy(data->buffer, buffer, receiving_msg);
        data->table = table;
        data->server_ip_or_domain_name = server_ip_or_domain_name;

        if (pthread_create(&tid, NULL, process_dns_message_thread, data) == 0) {
            pthread_detach(tid);
        } else {
            perror("pthread_create"); free(data); sem_post(thread_sem); }
    }

    sem_destroy(thread_sem);
    close(sockfd);
    return 0;
}
