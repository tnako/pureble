#ifndef OBJ_POLLER_H_
#define OBJ_POLLER_H_

#include "pcore/types.h"
#include "pobj/timer.h"

#include <sys/epoll.h>
#include <signal.h>

#define POBJIN EPOLLIN
#define POBJOUT EPOLLOUT
#define POBJET EPOLLET


typedef struct {
    pobj_timer timer;
    void (*callback)();
} pobj_internal_timer;

typedef struct {
    bool broken;
    bool threaded;
    int epoll_fd;
    pint32 num_epoll_events;
    puint32 ref_count;
    puint32 max_epoll_events;
    struct epoll_event* epoll_events;
    pobj_internal_timer internal_timer;
    void (*signals_callback[_NSIG])();
    int signals_fd;

    // ToDo: список подписчиков

} pobj_loop;

typedef void(*pobj_event_emit)(pobj_loop* loop, const puint32 epoll_events);



pobj_loop* pobj_create(puint32 max_epoll_events, bool threaded);
void pobj_destroy(pobj_loop **loop);

void pobj_run(pobj_loop*);

bool pobj_register_event(pobj_loop *loop, const pobj_event_emit callback, const pint32 eventfd, const pint32 type);
bool pobj_unregister_event(pobj_loop *loop, const pint32 eventfd);

bool pobj_internal_timer_start(pobj_loop *loop, const puint8 repeating, const struct timespec time, void (*callback)());
void pobj_internal_timer_stop(pobj_loop *loop);


#endif
