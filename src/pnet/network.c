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


bool pnet_internal_create_socket(pnet_socket *socket, pint16 type)
{
    if (unlikely(!socket)) {
        plog_error("pnet_internal_create_socket() нет объекта socket");
        return false;
    }

    int sock = nn_socket(AF_SP_RAW, type);
    if (unlikely(sock < 0)) {
        plog_error("pnet_internal_create_socket() не удалось создать сокет | %s (%d)", nn_strerror(nn_errno()), nn_errno());
        return false;
    }

    socket->socket_fd = sock;

    int val = 1;
    if (unlikely(nn_setsockopt(socket->socket_fd, NN_TCP, NN_TCP_NODELAY, &val, sizeof(val)))) {
        plog_error("pnet_internal_create_socket() не удалось выставить NN_TCP_NODELAY | %s (%d)", nn_strerror(nn_errno()), nn_errno());
        return false;
    }

    val = 0;
    if (unlikely(nn_setsockopt(socket->socket_fd, NN_SOL_SOCKET, NN_LINGER, &val, sizeof(val)))) {
        plog_error("pnet_internal_create_socket() не удалось выставить NN_LINGER | %s (%d)", nn_strerror(nn_errno()), nn_errno());
        return false;
    }

    val = 10000;
    if (unlikely(nn_setsockopt(socket->socket_fd, NN_SOL_SOCKET, NN_RECONNECT_IVL_MAX, &val, sizeof(val)))) {
        plog_error("pnet_internal_create_socket() не удалось выставить NN_RECONNECT_IVL_MAX | %s (%d)", nn_strerror(nn_errno()), nn_errno());
        return false;
    }

    return true;
}

bool pnet_internal_get_socketfd(pnet_socket *socket)
{
    size_t sd_size = sizeof(int);

    if (unlikely(nn_getsockopt(socket->socket_fd, NN_SOL_SOCKET, NN_RCVFD, &socket->recv_fd, &sd_size))) {
        plog_error("pnet_server_start() не удалось получить NN_RCVFD | %s (%d)", nn_strerror(nn_errno()), nn_errno());
        return false;
    }

    if (unlikely(nn_getsockopt(socket->socket_fd, NN_SOL_SOCKET, NN_SNDFD, &socket->send_fd, &sd_size))) {
        plog_error("pnet_server_start() не удалось получить NN_SNDFD | %s (%d)", nn_strerror(nn_errno()), nn_errno());
        return false;
    }

    return true;
}


bool pnet_server_start(pnet_socket *socket, const char *address)
{
    if (unlikely(!address)) {
        plog_error("pnet_server_start() нет строки адреса");
        return false;
    }

    if (unlikely(!pnet_internal_create_socket(socket, NN_REP))) {
        return false;
    }

    socket->endpoint_id = nn_bind(socket->socket_fd, address);
    if (unlikely(socket->endpoint_id < 0)) {
        plog_error("pnet_server_start() не удалось bind() | %s (%d)", nn_strerror(nn_errno()), nn_errno());
        return false;
    }

    if (unlikely(!pnet_internal_get_socketfd(socket))) {
        return false;
    }

    socket->started = true;

    return true;
}


void pnet_socket_destroy(pnet_socket *socket)
{
    if (unlikely(!socket)) {
        plog_error("pnet_socket_destroy() нет объекта socket");
        return;
    }

    if (unlikely(!socket->socket_fd)) {
        return;
    }

    nn_shutdown(socket->socket_fd, 0);
    nn_close(socket->socket_fd);
    socket->socket_fd = 0;
    socket->endpoint_id = 0;
    socket->started = false;
}


pint32 pnet_recv(pnet_socket *socket, char **buffer)
{
    if (unlikely(!socket)) {
        plog_error("pnet_recv() нет объекта socket");
        return -1;
    }

    if (unlikely(!buffer)) {
        plog_error("pnet_recv() buffer неверен");
        return -1;
    }

    //int bytes = nn_recv(socket->socket_fd, buffer, NN_MSG, 0);

    struct nn_msghdr hdr;
    struct nn_iovec iov [2];
    char buf0 [10] = { 0 };
    char buf1 [6] = { 0 };
    iov [0].iov_base = buf0;
    iov [0].iov_len = sizeof (buf0);
    iov [1].iov_base = buf1;
    iov [1].iov_len = sizeof (buf1);
    memset (&hdr, 0, sizeof (hdr));
    hdr.msg_iov = iov;
    hdr.msg_iovlen = 2;
    int bytes = nn_recvmsg(socket->socket_fd, &hdr, 0);

    printf("%d | %s | %s\n", bytes, buf0, buf1);
    *buffer = strdup(buf0);

    return bytes;
}


pint32 pnet_send(pnet_socket *socket, char *buffer)
{
    if (unlikely(!socket)) {
        plog_error("pnet_send() нет объекта socket");
        return -1;
    }

    if (unlikely(!buffer)) {
        plog_error("pnet_send() нет buffer");
        return -1;
    }

    int bytes = nn_send(socket->socket_fd, buffer, strlen(buffer), 0);

    return bytes;
}

bool pnet_client_start(pnet_socket *socket, const char *address)
{
    if (unlikely(!address)) {
        plog_error("pnet_server_start() нет строки адреса");
        return false;
    }

    if (unlikely(!pnet_internal_create_socket(socket, NN_REQ))) {
        return false;
    }

    socket->endpoint_id = nn_connect(socket->socket_fd, address);
    if (unlikely(socket->endpoint_id < 0)) {
        plog_error("pnet_server_start() не удалось bind() | %s (%d)", nn_strerror(nn_errno()), nn_errno());
        return false;
    }

    if (unlikely(!pnet_internal_get_socketfd(socket))) {
        return false;
    }

    socket->started = true;

    return true;
}

