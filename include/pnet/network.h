#ifndef NET_NETWORK_H_
#define NET_NETWORK_H_

#include "pcore/types.h"


typedef struct {
    int socket_fd;
    int recv_fd;
    int send_fd;
    int endpoint_id;

    bool started;
} pnet_socket;

bool pnet_server_start(pnet_socket *socket, const char *address);

bool pnet_client_start(pnet_socket *socket, const char *address);

void pnet_socket_destroy(pnet_socket *socket);


pint32 pnet_recv(pnet_socket *socket, char **buffer);
pint32 pnet_send(pnet_socket *socket, char *buffer);

#endif
