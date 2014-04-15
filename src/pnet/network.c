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


//  MDP/Server commands, as strings
#define MDPW_HELLO          "PW_HI"
#define MDPW_REQUEST        "PW_REQ"
#define MDPW_REPLY          "PW_REP"
#define MDPW_HEARTBEAT      "PW_HB"

#define HEARTBEAT_EXPIRY    60000 * 5 // 5 Minute


struct client_t {
    zctx_t *ctx;                //  Our context
    void *socket;               //  Socket for clients & workers
    int socket_fd;               //  Socket for clients & workers
};

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

typedef struct {
    char *id_string;            //  Identity of worker as string
    zframe_t *identity;         //  Identity frame for routing
    service_t *service;         //  Owning service, if known
    int64_t expiry;             //  When worker expires, if no heartbeat
} worker_t;


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
    broker = pmalloc(sizeof(pnet_broker));
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

static void pnet_broker_internal_client_return_msg(const pnet_broker *broker, zmsg_t *msg, zframe_t *service_frame, const char *return_code)
{
    zframe_t *client = zmsg_unwrap(msg);
    zmsg_pushstr(msg, return_code);
    zmsg_push(msg, zframe_dup(service_frame));
    zmsg_pushstr(msg, MDPE_CLIENT);
    zmsg_wrap(msg, client);

    plog_dbg("Send msg to client:");
    zmsg_dump(msg);

    int ret = zmsg_send(&msg, broker->socket);
    if (ret < 0) {
        printf("send %d | %d | %s\n", ret, zmq_errno(), zmq_strerror(zmq_errno()));
    }
}

static void pnet_broker_internal_worker_delete(const pnet_broker *broker, worker_t *worker)
{
    if (worker->service) {
        zlist_remove (worker->service->waiting, worker);
        worker->service->workers--;
    }

    zlist_remove(broker->waiting, worker);
    zhash_delete(broker->workers, worker->id_string);
}

void pnet_broker_purge_workers(const pnet_broker *broker)
{
    worker_t *worker = (worker_t *) zlist_first(broker->waiting);
    pint64 cur_time = zclock_time();
    while (worker) {
        if (cur_time <= worker->expiry) {
            break;
        }

        plog_dbg("Удаляем мёртвого работника: %s", worker->id_string);

        pnet_broker_internal_worker_delete(broker, worker);
        worker = (worker_t *) zlist_first(broker->waiting);
    }
}

static void pnet_broker_internal_worker_send(const pnet_broker *broker, worker_t *worker, char *command, zmsg_t *msg)
{
    zmsg_pushstr(msg, command);
    zmsg_pushstr(msg, MDPE_WORKER);

    zmsg_wrap(msg, zframe_dup(worker->identity));

    plog_dbg("Send to worker:");
    zmsg_dump(msg);
    zmsg_send(&msg, broker->socket);
}

static void pnet_broker_internal_service_dispatch(const pnet_broker *broker, service_t *service, zmsg_t *msg)
{
    if (msg) {
        zlist_append(service->requests, msg);
    }

    while (zlist_size(service->waiting) && zlist_size(service->requests)) {
        worker_t *worker = zlist_pop(service->waiting);
        zlist_remove(broker->waiting, worker);
        zmsg_t *msg = zlist_pop(service->requests);
        pnet_broker_internal_worker_send(broker, worker, MDPW_REQUEST, msg);
    }
}

static void pnet_broker_internal_client_read(const pnet_broker *broker, zframe_t *sender, zmsg_t *msg)
{
    if (unlikely(zmsg_size(msg) < 2)) {
        plog_warn("pnet_broker_internal_client_read() size = %d", zmsg_size(msg));
        zmsg_destroy(&msg);
        return;
    }

    zframe_t *service_frame = zmsg_pop(msg);

    zmsg_wrap(msg, zframe_dup(sender));

    char *name = zframe_strdup(service_frame);
    service_t *service = (service_t *)zhash_lookup(broker->services, name);

    if (unlikely(service == NULL)) {
        pnet_broker_internal_client_return_msg(broker, msg, service_frame, "404"); // Not Found
    } else if (likely(service->workers)) {
        pnet_broker_internal_service_dispatch(broker, service, msg);
    } else {
        pnet_broker_internal_client_return_msg(broker, msg, service_frame, "503"); // Service Unavailable
    }

    zframe_destroy(&service_frame);
}

static void pnet_broker_internal_worker_destroy(void *argument)
{
    worker_t *self = (worker_t *) argument;
    zframe_destroy(&self->identity);
    pfree(&self->id_string);
    pfree(&self);
}

static void pnet_broker_internal_service_destroy(void *argument)
{
    service_t *service = (service_t *) argument;

    while (zlist_size(service->requests)) {
        zmsg_t *msg = zlist_pop(service->requests);
        zmsg_destroy(&msg);
    }

    zlist_destroy(&service->requests);
    zlist_destroy(&service->waiting);
    pfree(&service->name);
    pfree(&service);
}

static void pnet_broker_internal_worker_waiting(const pnet_broker *broker, worker_t *worker)
{
    zlist_append(broker->waiting, worker);
    zlist_append(worker->service->waiting, worker);
    worker->expiry = zclock_time() + HEARTBEAT_EXPIRY;
    pnet_broker_internal_service_dispatch(broker, worker->service, NULL);
}

static void pnet_broker_internal_worker_read(const pnet_broker *broker, zframe_t *sender, zmsg_t *msg)
{
    if (unlikely(zmsg_size(msg) < 1)) {
        plog_warn("pnet_broker_internal_worker_read() zmsg_size = %d", zmsg_size(msg));
        zmsg_destroy(&msg);
        return;
    }

    zframe_t *command = zmsg_pop(msg);
    char *id_string = zframe_strhex(sender);
    int worker_ready = (zhash_lookup(broker->workers, id_string) != NULL);



    worker_t *worker = (worker_t *) zhash_lookup(broker->workers, id_string);

    if (worker == NULL) {
        worker = (worker_t *) pmalloc(sizeof(worker_t));
        worker->id_string = id_string;
        worker->identity = zframe_dup(sender);
        zhash_insert(broker->workers, id_string, worker);
        zhash_freefn(broker->workers, id_string, pnet_broker_internal_worker_destroy);
        plog_dbg("Новый рабочий зарегистрирован: %x", zframe_data(worker->identity));
    } else {
        pfree(&id_string);
    }


    if (worker_ready) {
        worker->expiry = zclock_time () + HEARTBEAT_EXPIRY;
    } else {
        // Attach worker to service and mark as idle
        zframe_t *service_frame = zmsg_pop(msg);
        char *name = zframe_strdup(service_frame);

        worker->service = (service_t *) pmalloc(sizeof(service_t));
        worker->service->name = name;
        worker->service->requests = zlist_new();
        worker->service->waiting = zlist_new();
        zhash_insert(broker->services, name, worker->service);
        zhash_freefn(broker->services, name, pnet_broker_internal_service_destroy);
        plog_dbg("Новый сервис зарегистрирован: %s", name);

        worker->service->workers++;
        pnet_broker_internal_worker_waiting(broker, worker);
        zframe_destroy(&service_frame);
    }



    if (zframe_streq(command, MDPW_HELLO)) {

        plog_dbg("Получен MDPW_HELLO от %s", worker->id_string);
        zmsg_destroy(&msg);

    } else if (zframe_streq(command, MDPW_REPLY)) {

        plog_dbg("Получен MDPW_REPLY от %s", worker->id_string);
        zframe_t *client = zmsg_unwrap(msg);
        zmsg_pushstr(msg, worker->service->name);
        zmsg_pushstr(msg, MDPE_CLIENT);
        zmsg_wrap(msg, client);
        zmsg_send(&msg, broker->socket);
        pnet_broker_internal_worker_waiting(broker, worker);

    } else if (zframe_streq(command, MDPW_HEARTBEAT)) {

        plog_dbg("Получен MDPW_HEARTBEAT от %s", worker->id_string);
        zmsg_pushstr(msg, MDPW_HEARTBEAT);
        zmsg_pushstr(msg, MDPE_WORKER);
        zmsg_wrap(msg, zframe_dup(sender));
        zmsg_send(&msg, broker->socket);

    } else {

        plog_warn("Неверный тип действия %x", (char *)zframe_data(command));
        zmsg_dump(msg);
        zmsg_destroy(&msg);
    }

    pfree(&command);
}

bool pnet_broker_readmsg(const pnet_broker *broker)
{
    if (unlikely(!broker)) {
        plog_error("pnet_broker_readmsg() нету объекта pnet_broker");
        return false;
    }

    zmsg_t *msg = zmsg_recv_nowait(broker->socket);

    if (!msg) {
        return false;
    }

    plog_dbg("Got msg:");
    zmsg_dump(msg);

    zframe_t *sender = zmsg_pop(msg);
    zframe_t *empty  = zmsg_pop(msg);
    zframe_t *header = zmsg_pop(msg);

    if (zframe_streq(header, MDPE_CLIENT)) {
        plog_dbg("Получено сообщение от клиента");
        pnet_broker_internal_client_read(broker, sender, msg);
    } else if (zframe_streq(header, MDPE_WORKER)) {
        plog_dbg("Получено сообщение от работника");
        pnet_broker_internal_worker_read(broker, sender, msg);

    } else {
        plog_warn("Неверное сообщение: %x | %s", zframe_data(sender), (char *)zframe_data(header));
        zmsg_dump(msg);
        zmsg_destroy(&msg);
    }

    zframe_destroy(&sender);
    zframe_destroy(&empty);
    zframe_destroy(&header);

    return true;
}

bool pnet_client_connect(pnet_client **client_p, const char *address)
{
    if (unlikely(*client_p)) {
        plog_error("pnet_client_connect() объект pnet_client уже существует");
        return false;
    }

    if (unlikely(!address)) {
        plog_error("pnet_client_connect() нет объекта address");
        return false;
    }

    pnet_client *client;
    client = pmalloc(sizeof(pnet_client));
    client->ctx = zctx_new();

    if (unlikely(!client->ctx)) {
        plog_error("pnet_client_connect() не удалось создать контекст");
        pfree(&client);
        return false;
    }

    client->socket = zsocket_new(client->ctx, ZMQ_DEALER);

    if (unlikely(!client->socket)) {
        plog_error("pnet_client_connect() не удалось создать socket");
        zctx_destroy(&client->ctx);
        pfree(&client);
        return false;
    }

    if (unlikely(zmq_connect(client->socket, address))) {
        plog_error("pnet_client_connect() не удалось connect %d | %s", zmq_errno(), zmq_strerror(zmq_errno()));
        zctx_destroy(&client->ctx);
        pfree(&client);
        return false;
    }

    size_t sfd_size = sizeof(client->socket_fd);
    if (unlikely(zmq_getsockopt(client->socket, ZMQ_FD, &client->socket_fd, &sfd_size) < 0)) {
        plog_error("pnet_client_connect() не удалось zmq_getsockopt");
        zctx_destroy(&client->ctx);
        pfree(&client);
        return false;
    }

    *client_p = client;

    return true;
}

int pnet_client_getfd(const pnet_client *client)
{
    if (unlikely(!client)) {
        plog_error("pnet_client_getfd() нету объекта pnet_client");
        return -1;
    }

    return client->socket_fd;
}


pnet_message *pnet_message_new()
{
    return zmsg_new();
}


void pnet_message_add(pnet_message *mes, const char *string)
{
    if (unlikely(!mes)) {
        plog_error("pnet_message_add() нету объекта pnet_message");
        return;
    }

    zmsg_addstr(mes, string);
}


void pnet_client_message_send(const pnet_client *client, pnet_message *mes, const char *service)
{
    if (unlikely(!client)) {
        plog_error("pnet_client_message_send() нету объекта pnet_client");
        return;
    }

    if (unlikely(!mes)) {
        plog_error("pnet_client_message_send() нету объекта pnet_message");
        return;
    }

    if (unlikely(!service)) {
        plog_error("pnet_client_message_send() нету имени службы");
        return;
    }

    zmsg_pushstr(mes, service);
    zmsg_pushstr(mes, MDPE_CLIENT);
    zmsg_pushstr(mes, "");
    zmsg_send (&mes, client->socket);
}


bool pnet_client_message_read(const pnet_client *client, pnet_message **mes)
{
    if (unlikely(!client)) {
        plog_error("pnet_client_message_read() нету объекта pnet_client");
        return false;
    }

    if (unlikely(*mes)) {
        plog_error("pnet_client_message_read() объект pnet_message существует");
        return false;
    }

    pint32 zmq_events = 0;
    size_t opt_len = sizeof(zmq_events);

    if (unlikely(zmq_getsockopt(client->socket, ZMQ_EVENTS, &zmq_events, &opt_len) < 0)) {
        plog_error("pnet_client_message_read() get zmq events failed, err: %s", zmq_strerror(errno));
        return false;
    }

    if (zmq_events & ZMQ_POLLIN) {
        pnet_message *new_msg = zmsg_recv_nowait(client->socket);

        if (!new_msg) {
            return false;
        }

        plog_dbg("Client fot msg:");
        zmsg_dump(new_msg);

        zframe_t *empty  = zmsg_pop(new_msg);
        zframe_destroy(&empty);

        zframe_t *header = zmsg_pop(new_msg);

        if (unlikely(!zframe_streq(header, MDPE_CLIENT))) {
            zframe_destroy(&header);
            return false;
        }

        zframe_destroy(&header);

        *mes = new_msg;
        return true;
    }

    return false;
}


char *pnet_message_get(pnet_message **mes)
{
    if (unlikely(!(*mes))) {
        plog_error("pnet_message_get() нету объекта pnet_message");
        return NULL;
    }

    char *line = zmsg_popstr(*mes);

    if (!line) {
        zmsg_destroy(mes);
    }

    return line;
}
