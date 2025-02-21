#ifndef __NET_H
#define __NET_H

#include <netinet/in.h>

extern size_t g_bytes_sent;
extern size_t g_bytes_received;

int
net_send(int socket, const void *buffer, size_t length, int flags);

int
net_recv(int socket, void *buffer, size_t length, int flags);

int
net_send_compressed(int socket, const void *buffer, size_t length, int flags);

int
net_recv_compressed(int socket, void *buffer, size_t length, int flags);

int
net_init_server(const char *addr, const char *port);

int
net_server_accept(int sockfd);

int
net_init_client(const char *addr, const char *port);


#endif
