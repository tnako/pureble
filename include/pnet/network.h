#ifndef NET_NETWORK_H_
#define NET_NETWORK_H_

#include "pcore/types.h"
#include "pobj/object.h"


typedef struct broker_t pnet_broker;



bool pnet_broker_start(pnet_broker **broker_p, const char *address);
bool pnet_broker_stop(pnet_broker **broker_p);

pint32 pnet_broker_check_event(const pnet_broker *broker);
void pnet_broker_purge_workers(const pnet_broker *broker);
bool pnet_broker_readmsg(const pnet_broker *broker);

bool pnet_broker_register(pnet_broker *broker, pobj_loop *loop, void (*callback)());


#endif
