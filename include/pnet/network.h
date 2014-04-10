#ifndef NET_NETWORK_H_
#define NET_NETWORK_H_

#include "pcore/types.h"


typedef struct {
    int socket_fd;
    int recv_fd;
    int send_fd;

    bool started;
} pnet_socket;

bool pnet_server_start(pnet_socket *socket, const char *address);
void pnet_server_stop(pnet_socket *socket);

#endif
