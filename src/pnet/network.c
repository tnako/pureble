#ifdef MODULE_NAME
#undef MODULE_NAME
#endif

#define MODULE_NAME "pnet/network"


#include "plog/log.h"
#include "pnet/network.h"
#include "pcore/internal.h"

#include <czmq.h>


#define MDPE_CLIENT         "PC01"
#define MDPE_BROKER         "PB01"
#define MDPE_WORKER         "PW01"



struct broker_t {
    zctx_t *ctx;                //  Our context
    void *socket;               //  Socket for clients & workers
    int socket_fd;               //  Socket for clients & workers
    zhash_t *services;          //  Hash of known services
    zhash_t *workers;           //  Hash of known workers
    zlist_t *waiting;           //  List of waiting workers
};

typedef struct {
    char *name;                 //  Service name
    zlist_t *requests;          //  List of client requests
    zlist_t *waiting;           //  List of waiting workers
    size_t workers;             //  How many workers we have
} service_t;



bool pnet_broker_start(pnet_broker **broker_p, const char *address)
{
    if (unlikely(*broker_p)) {
        plog_error("pnet_broker_start() объект pnet_broker уже существует");
        return false;
    }

    if (unlikely(!address)) {
        plog_error("pnet_broker_start() нет объекта address");
        return false;
    }

    pnet_broker *broker;
    broker = pmalloc(sizeof(struct broker_t));
    broker->ctx = zctx_new();

    if (unlikely(!broker->ctx)) {
        plog_error("pnet_broker_start() не удалось создать контекст");
        pfree(&broker);
        return false;
    }

    broker->socket = zsocket_new(broker->ctx, ZMQ_ROUTER);

    if (unlikely(!broker->socket)) {
        plog_error("pnet_broker_start() не удалось создать socket");
        zctx_destroy(&broker->ctx);
        pfree(&broker);
        return false;
    }

    broker->services = zhash_new();
    broker->workers = zhash_new();
    broker->waiting = zlist_new();

    pint32 port = zsocket_bind(broker->socket, address);

    if (unlikely(port < 1)) {
        plog_error("pnet_broker_start() не удалось bind на порт %d | %d | %s", port, zmq_errno(), zmq_strerror(zmq_errno()));
        zctx_destroy(&broker->ctx);
        pfree(&broker);
        return false;
    }

    size_t sfd_size = sizeof(broker->socket_fd);
    if (unlikely(zmq_getsockopt(broker->socket, ZMQ_FD, &broker->socket_fd, &sfd_size) < 0)) {
        plog_error("pnet_broker_start() не удалось zmq_getsockopt");
        zctx_destroy(&broker->ctx);
        pfree(&broker);
        return false;
    }

    *broker_p = broker;

    return true;
}

bool pnet_broker_stop(pnet_broker **broker_p)
{
    pnet_broker *broker = *broker_p;

    if (unlikely(!broker)) {
        plog_error("pnet_broker_stop() нету объекта pnet_broker");
        return false;
    }

    zctx_destroy(&broker->ctx);
    zhash_destroy(&broker->services);
    zhash_destroy(&broker->workers);
    zlist_destroy(&broker->waiting);
    pfree(broker_p);

    return true;
}

bool pnet_broker_register(pnet_broker *broker, pobj_loop *loop, void (*callback)())
{
    if (unlikely(!broker)) {
        plog_error("pnet_broker_register() нету объекта pnet_broker");
        return false;
    }

    if (unlikely(!loop)) {
        plog_error("pnet_broker_register() нету объекта pobj_loop");
        return false;
    }

    if (unlikely(!callback)) {
        plog_error("pnet_broker_register() нету объекта callback");
        return false;
    }

    if (unlikely(!pobj_register_event(loop, callback, broker->socket_fd, POBJIN | POBJET))) {
        plog_error("pnet_broker_register() не удалось зарегистрировать событие");
        return false;
    }

    return true;
}

pint32 pnet_broker_check_event(const pnet_broker *broker)
{
    if (unlikely(!broker)) {
        plog_error("pnet_broker_register() нету объекта pnet_broker");
        return -1;
    }

    pint32 zmq_events = 0;
    size_t opt_len = sizeof(zmq_events);

    if (unlikely(zmq_getsockopt(broker->socket, ZMQ_EVENTS, &zmq_events, &opt_len) < 0)) {
        plog_error("get zmq events failed, err: %s", zmq_strerror(errno));
        return -1;
    }

    pint32 ret = 0;

    if (zmq_events & ZMQ_POLLIN) {
        ret |= POBJIN;
    }
    if (zmq_events & ZMQ_POLLOUT) {
        ret |= POBJOUT;
    }

    return ret;
}

static void pnet_broker_internal_client_read(const pnet_broker *broker, zframe_t *sender, zmsg_t *msg)
{
    if (unlikely(zmsg_size(msg) < 2)) {
        plog_warn("pnet_broker_internal_client_read() zmsg_size = ", zmsg_size(msg));
        zmsg_destroy(&msg);
        return;
    }

    zframe_t *service_frame = zmsg_pop(msg);

    zmsg_wrap(msg, zframe_dup(sender));

    char *name = (char *)zframe_data(service_frame);
    service_t *service = (service_t *)zhash_lookup(broker->services, name);

    char *return_code;
    if (unlikely(service == NULL)) {
        return_code = "404"; // Not Found
    } else if (likely(service->workers)) {
        return_code = "200"; // OK // ToDo: send to worker
    } else {
        return_code = "503"; // Service Unavailable
    }

    zframe_reset(zmsg_last(msg), return_code, strlen(return_code));

    zframe_t *client = zmsg_unwrap(msg);
    zmsg_push(msg, zframe_dup(service_frame));
    zmsg_pushstr(msg, MDPE_CLIENT);
    zmsg_wrap(msg, client);
    zmsg_send(&msg, broker->socket);

    zframe_destroy(&service_frame);
}

void pnet_broker_readmsg(const pnet_broker *broker)
{
    if (unlikely(!broker)) {
        plog_error("pnet_broker_readmsg() нету объекта pnet_broker");
        return;
    }

    zmsg_t *msg = zmsg_recv(broker->socket);

    if (unlikely(!msg)) {
        return;
    }

    plog_dbg("Got msg:");
    zmsg_dump(msg);

    zframe_t *sender = zmsg_pop(msg);
    zframe_t *empty  = zmsg_pop(msg);
    zframe_t *header = zmsg_pop(msg);

    if (zframe_streq(header, MDPE_CLIENT)) {
        pnet_broker_internal_client_read(broker, sender, msg);
    } else if (zframe_streq(header, MDPE_WORKER)) {
        //worker_callback(sender, msg);
        // ToDo: worker
    } else {
        plog_warn("Invalid message:");
        zmsg_dump(msg);
        zmsg_destroy(&msg);
    }

    zframe_destroy(&sender);
    zframe_destroy(&empty);
    zframe_destroy(&header);
}
