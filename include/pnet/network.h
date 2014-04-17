#ifndef NET_NETWORK_H_
#define NET_NETWORK_H_

#include "pcore/types.h"
#include "pobj/object.h"


#define HEARTBEAT_EXPIRY    1000 * 10 // 10 Seconds
#define HEARTBEAT_MISS             3 // Can make 3 miss


typedef struct broker_t pnet_broker;
typedef struct client_t pnet_client;
typedef struct _zmsg_t pnet_message;


bool pnet_broker_start(pnet_broker **broker_p, const char *address);
bool pnet_broker_stop(pnet_broker **broker_p);


pint32 pnet_broker_check_event(const pnet_broker *broker);
void pnet_broker_purge_workers(const pnet_broker *broker);
bool pnet_broker_readmsg(const pnet_broker *broker);


bool pnet_broker_register(pnet_broker *broker, pobj_loop *loop, void (*callback)());
bool pnet_client_register(pnet_client *client, pobj_loop *loop, void (*callback)());
bool pnet_worker_register(pnet_client *client, pobj_loop *loop, void (*callback)());




bool pnet_client_start(pnet_client **client_p, const char *address);
bool pnet_client_stop(pnet_client **client_p);
int pnet_client_getfd(const pnet_client *client);



pnet_message* pnet_message_new();
void pnet_message_add(pnet_message *mes, const char *string);
char *pnet_message_get(pnet_message **mes);
void pnet_message_destroy(pnet_message **mes);


void pnet_client_message_send(const pnet_client *client, pnet_message *mes, const char *service);
bool pnet_client_message_read(const pnet_client *client, pnet_message **mes);

void pnet_worker_message_send(const pnet_client *client, pnet_message *mes, const char *service);
bool pnet_worker_message_read(pnet_client *client, pnet_message **mes);

void pnet_worker_send_hello(const pnet_client *client, const char *string);
bool pnet_worker_send_heartbeat(pnet_client *client);

pnet_message *pnet_worker_reply_init(pnet_message *request);
void pnet_worker_reply_send(const pnet_client *client, pnet_message *reply);

#endif
