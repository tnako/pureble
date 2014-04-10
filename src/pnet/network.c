#ifdef MODULE_NAME
#undef MODULE_NAME
#endif

#define MODULE_NAME "pnet/network"


#include "plog/log.h"
#include "pnet/network.h"
#include "pcore/internal.h"

#include <nanomsg/nn.h>
#include <nanomsg/reqrep.h>
#include <nanomsg/tcp.h>


bool pnet_server_start(pnet_socket *socket, const char *address)
{
    if (unlikely(!socket)) {
        plog_error("pnet_server_start() нет объекта socket");
        return false;
    }

    if (unlikely(!address)) {
        plog_error("pnet_server_start() нет строки адреса");
        return false;
    }

    int sock = nn_socket (AF_SP, NN_REP);
    if (unlikely(sock < 1)) {
        plog_error("pnet_server_start() не удалось создать сокет | %s (%d)", nn_strerror(nn_errno()), nn_errno());
        return false;
    }

    socket->socket_fd = sock;

    int val = 1;
    if (unlikely(!nn_setsockopt(socket->socket_fd, NN_TCP, NN_TCP_NODELAY, &val, sizeof(val)))) {
        plog_error("pnet_server_start() не удалось выставить NN_TCP_NODELAY | %s (%d)", nn_strerror(nn_errno()), nn_errno());
        return false;
    }

    val = 0;
    if (unlikely(!nn_setsockopt(socket->socket_fd, NN_SOL_SOCKET, NN_LINGER, &val, sizeof(val)))) {
        plog_error("pnet_server_start() не удалось выставить NN_LINGER | %s (%d)", nn_strerror(nn_errno()), nn_errno());
        return false;
    }

    val = 10000;
    if (unlikely(!nn_setsockopt(socket->socket_fd, NN_SOL_SOCKET, NN_RECONNECT_IVL_MAX, &val, sizeof(val)))) {
        plog_error("pnet_server_start() не удалось выставить NN_RECONNECT_IVL_MAX | %s (%d)", nn_strerror(nn_errno()), nn_errno());
        return false;
    }

    if (unlikely(!nn_bind(socket->socket_fd, address))) {
        plog_error("pnet_server_start() не удалось bind() | %s (%d)", nn_strerror(nn_errno()), nn_errno());
        return false;
    }


    size_t sd_size = sizeof(int);

    if (unlikely(!nn_getsockopt(socket->socket_fd, NN_SOL_SOCKET, NN_RCVFD, &socket->recv_fd, &sd_size))) {
        plog_error("pnet_server_start() не удалось получить NN_RCVFD | %s (%d)", nn_strerror(nn_errno()), nn_errno());
        return false;
    }

    if (unlikely(!nn_getsockopt(socket->socket_fd, NN_SOL_SOCKET, NN_SNDFD, &socket->send_fd, &sd_size))) {
        plog_error("pnet_server_start() не удалось получить NN_SNDFD | %s (%d)", nn_strerror(nn_errno()), nn_errno());
        return false;
    }

    socket->started = true;

    return true;
}


void pnet_server_stop(pnet_socket *socket)
{
    if (unlikely(!socket)) {
        plog_error("pnet_server_stop() нет объекта socket");
        return;
    }

    if (unlikely(!socket->socket_fd)) {
        return;
    }

    nn_close(socket->socket_fd);
    socket->socket_fd = 0;
}
