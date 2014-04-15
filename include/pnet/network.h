#ifndef NET_NETWORK_H_
#define NET_NETWORK_H_

#include "pcore/types.h"
#include "pobj/object.h"


typedef struct broker_t pnet_broker;
typedef struct client_t pnet_client;
typedef struct _zmsg_t pnet_message;


bool pnet_broker_start(pnet_broker **broker_p, const char *address);
bool pnet_broker_stop(pnet_broker **broker_p);


pint32 pnet_broker_check_event(const pnet_broker *broker);
void pnet_broker_purge_workers(const pnet_broker *broker);
bool pnet_broker_readmsg(const pnet_broker *broker);


bool pnet_broker_register(pnet_broker *broker, pobj_loop *loop, void (*callback)());




bool pnet_client_connect(pnet_client **client_p, const char *address);
int pnet_client_getfd(const pnet_client *client);



pnet_message* pnet_message_new();
void pnet_message_add(pnet_message *mes, const char *string);
char *pnet_message_get(pnet_message **mes);


void pnet_client_message_send(const pnet_client *client, pnet_message *mes, const char *service);
bool pnet_client_message_read(const pnet_client *client, pnet_message **mes);

#endif
