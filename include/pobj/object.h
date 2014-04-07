#ifndef OBJ_POLLER_H_
#define OBJ_POLLER_H_

#include "pcore/types.h"

#include <sys/epoll.h>

#define POBJIN EPOLLIN
#define POBJOUT EPOLLOUT

typedef struct {
  bool broken;
  puint32 ref_count;

  int epoll_fd;

  puint32 max_epoll_events;
  pint32 num_epoll_events;
  struct epoll_event* epoll_events;

  // ToDo: список подписчиков
} pobj_loop;


typedef void(*pobj_event_emit)(pobj_loop* loop, puint32 epoll_events);



pobj_loop* pobj_create(puint32 max_epoll_events, pint8 threaded);
//ToDo: destroy

void pobj_run(pobj_loop*);

bool pobj_register_event(pobj_loop *loop, pobj_event_emit callback, pint32 event, pint32 type);
bool pobj_unregister_event(pobj_loop *loop, pint32 event);


#endif
