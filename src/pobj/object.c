#ifdef MODULE_NAME
#undef MODULE_NAME
#endif

#define MODULE_NAME "pobj/poller"

#include "pobj/object.h"
#include "plog/log.h"
#include "pcore/internal.h"

#include <errno.h>



pobj_loop *pobj_create(puint32 max_epoll_events, pint8 threaded)
{
    if (threaded) {
        plog_error("ToDo: Многопоточность");
    }

    puint32 s = 4;
    while(s < max_epoll_events) {
        s <<= 1; // x 2
    }
    max_epoll_events = s;

    pobj_loop *loop = pmalloc(sizeof(pobj_loop)+sizeof(struct epoll_event)*max_epoll_events);

    loop->max_epoll_events = max_epoll_events;
    loop->epoll_fd = epoll_create(1); // size ignored

    return loop;
}


void pobj_run(pobj_loop *loop)
{
    while (likely(!loop->broken)) {
        if (unlikely(loop->ref_count <= 0)) {
            plog_dbg("Poller ref_count <= 0");
            break;
        }

        loop->num_epoll_events = epoll_wait(loop->epoll_fd, loop->epoll_events, loop->max_epoll_events, -1);

        if (unlikely(loop->num_epoll_events <= 0)) {
            if (errno != EINTR) {
                plog_error("epoll_wait error: %d %s", errno, strerror(errno));
            }
            continue;
        }

        struct epoll_event* ev;
        pint32 i = loop->num_epoll_events;
        for (ev = loop->epoll_events; i > 0; i--, ev++) {
            if (unlikely(!ev->data.ptr)) {
                plog_error("epoll_events() нету callback");
                continue;
            }
            pobj_event_emit es = (pobj_event_emit)ev->data.ptr;
            es(loop, ev->events);
        }
    }
}

bool pobj_register_event(pobj_loop *loop, pobj_event_emit callback, pint32 event, pint32 type)
{
    if (unlikely(!loop)) {
        plog_error("pobj_register_event() нет объекта");
        return false;
    }

    if (unlikely(!callback)) {
        plog_error("pobj_register_event() нет возвращаемой функции");
        return false;
    }

    if (unlikely(!event)) {
        plog_error("pobj_register_event() event неверен");
        return false;
    }

    if (unlikely(!type)) {
        plog_error("pobj_register_event() нет типа срабатывания");
        return false;
    }

    struct epoll_event ev={type|EPOLLRDHUP|EPOLLHUP|EPOLLET, {.ptr = callback}};

    if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, event, &ev)==-1) {
        plog_error("epoll_ctl ADD error: %d %s", errno, strerror(errno));
        return false;
    }

    loop->ref_count++;
    return true;
}

bool pobj_unregister_event(pobj_loop *loop, pint32 event)
{
    if (unlikely(!loop)) {
        plog_error("pobj_unregister_event() нет объекта");
        return false;
    }

    if (unlikely(!event)) {
        plog_error("pobj_unregister_event() event неверен");
        return false;
    }


    if (unlikely(epoll_ctl(loop->epoll_fd, EPOLL_CTL_DEL, event, NULL)==-1)) {
        plog_error("epoll_ctl DEL error: %d %s", errno, strerror(errno));
    }


    loop->ref_count--;
    return true;
}
