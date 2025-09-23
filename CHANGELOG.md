ARG_PARSER
    parsing arguments from stdin
HASHTABLE
    stores undesirable domains for later search
    search part 
DNS_FORWARDER 
    recieving data on selected port
    parsing data 
    sending response if located in undesarablelist 
    sending  and recieving form  resolver
    sending data back to client
    support for more then one packet
    make support for more then one forwarding at same time (threes)

TODO
    `ctrl C` implementation ____DONE FOR NOW
    i dont know format i should send back to recipient
    tests
    documentation
    code clean up

ERR
    i had problem when masage was block problem didnt apper after another try with same input
    Domain itu.ypu.samsungelectronics.com is BLOCKED.
Sent blocked response (48 bytes) to client
==39799== Thread 2:
==39799== Invalid free() / delete / delete[] / realloc()
==39799==    at 0x484988F: free (in /usr/libexec/valgrind/vgpreload_memcheck-amd64-linux.so)
==39799==    by 0x10B940: process_dns_message_thread (in /home/tomas/projekty/ISA/dns)
==39799==    by 0x4900AA3: start_thread (pthread_create.c:447)
==39799==    by 0x498DA33: clone (clone.S:100)
==39799==  Address 0x4b6e074 is 4 bytes inside a block of size 32 alloc'd
==39799==    at 0x4846828: malloc (in /usr/libexec/valgrind/vgpreload_memcheck-amd64-linux.so)
==39799==    by 0x10BE7F: decode_dns_name (in /home/tomas/projekty/ISA/dns)
==39799==    by 0x10BFAB: parse_dns_question (in /home/tomas/projekty/ISA/dns)
==39799==    by 0x10B8C1: process_dns_message_thread (in /home/tomas/projekty/ISA/dns)
==39799==    by 0x4900AA3: start_thread (pthread_create.c:447)
==39799==    by 0x498DA33: clone (clone.S:100)
==39799== 