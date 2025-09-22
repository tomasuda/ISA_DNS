#ifndef ISA_DNS_FORWARDER_H
#define ISA_DNS_FORWARDER_H

#include "hashtable.h"
#include "main.h"
#include <stdint.h>

/* Struct for the DNS Header - NETWORK BYTE ORDER */
typedef struct header {
    uint16_t id;           // network byte order
    uint16_t flags;        // network byte order - use bit masking!
    uint16_t qdcount;      // network byte order
    uint16_t ancount;      // network byte order  
    uint16_t nscount;      // network byte order
    uint16_t arcount;      // network byte order
} HEADER;

/* Flags accessed through bit masks */
#define DNS_QR_MASK    0x8000
#define DNS_OPCODE_MASK 0x7800  
#define DNS_AA_MASK    0x0400
#define DNS_TC_MASK    0x0200
#define DNS_RD_MASK    0x0100
#define DNS_RA_MASK    0x0080
#define DNS_Z_MASK     0x0070
#define DNS_RCODE_MASK 0x000F

/* Struct for the flags for the DNS Question */
typedef struct q_flags {
    uint16_t qtype;
    uint16_t qclass;
} Q_FLAGS;

/* Struct for the flags for the DNS RRs */
typedef struct rr_flags {
    uint16_t type;
    uint16_t class;
    uint32_t ttl;
    uint16_t rdlength;
} RR_FLAGS;

// DNS question structure
typedef struct {
    char *qname;        // domain name
    Q_FLAGS flags;      // question flags
} dns_question_t;

// DNS resource record structure
typedef struct {
    char *name;         // domain name
    RR_FLAGS flags;     // resource record flags
    unsigned char *rdata; // resource data
} dns_rr_t;

// DNS message structure
typedef struct {
    HEADER header;
    dns_question_t *questions;
    dns_rr_t *answers;
    dns_rr_t *authorities;
    dns_rr_t *additionals;
} dns_message_t;


typedef struct {
    int sockfd;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len;
    ssize_t msg_len;
    unsigned char buffer[1024];
    ht_table_t table;
    char *server_ip_or_domain_name;
} thread_data_t;

int server_part(int port, ht_table_t table, char *server_ip_or_domain_name);
void bye_termination(Arguments *args, ht_table_t table);
#endif // ISA_DNS_FORWARDER_H