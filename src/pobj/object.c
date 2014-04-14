#ifdef MODULE_NAME
#undef MODULE_NAME
#endif

#define MODULE_NAME "pobj/object"

#include "pobj/object.h"
#include "plog/log.h"
#include "pcore/internal.h"

#include <errno.h>
#include <unistd.h>


typedef struct {
    pint32 eventfd;
    pint32 type;
} pobj_events;

pobj_events *pobj_internal_events;
puint32 pobj_internal_events_last_event;


pobj_loop *pobj_create(puint32 max_epoll_events, bool threaded)
{
    if (threaded) {
        plog_error("ToDo: Многопоточность");
    }

    puint32 s = 4;
    while(likely(s < max_epoll_events)) {
        s <<= 1; // x2
    }
    max_epoll_events = s;

    pobj_loop *loop = pmalloc(sizeof(pobj_loop)+sizeof(struct epoll_event)*max_epoll_events);
    pobj_internal_events = pmalloc(sizeof(pobj_events)*max_epoll_events);
    pobj_internal_events_last_event = 0;

    loop->epoll_events=(struct epoll_event*)((char*)(loop+1));
    loop->max_epoll_events = max_epoll_events;
    loop->epoll_fd = epoll_create(1); // size ignored
    loop->threaded = threaded;

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
        plog_dbg("epoll_wait() кол-во событий: %d", loop->num_epoll_events);

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

bool pobj_register_event(pobj_loop *loop, const pobj_event_emit callback, const pint32 eventfd, const pint32 type)
{
    if (unlikely(!loop)) {
        plog_error("pobj_register_event() нет объекта");
        return false;
    }

    if (unlikely(!callback)) {
        plog_error("pobj_register_event() нет возвращаемой функции");
        return false;
    }

    if (unlikely(!eventfd)) {
        plog_error("pobj_register_event() event неверен");
        return false;
    }

    if (unlikely(!type)) {
        plog_error("pobj_register_event() нет типа срабатывания");
        return false;
    }

    struct epoll_event ev={type|EPOLLRDHUP|EPOLLHUP, {.ptr = callback}};

    if (unlikely(epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, eventfd, &ev)==-1)) {
        plog_error("epoll_ctl ADD error: %d %s", errno, strerror(errno));
        return false;
    } else {
        plog_dbg("Цикл %x | Событие %d зарегистрировано на функцию %x с типом %x", loop, eventfd, callback, type);
    }

    pobj_internal_events[pobj_internal_events_last_event].eventfd = eventfd;
    pobj_internal_events[pobj_internal_events_last_event++].type = type;

    loop->ref_count++;
    return true;
}

bool pobj_unregister_event(pobj_loop *loop, const pint32 eventfd)
{
    if (unlikely(!loop)) {
        plog_error("pobj_unregister_event() нет объекта");
        return false;
    }

    if (unlikely(!eventfd)) {
        plog_error("pobj_unregister_event() event неверен");
        return false;
    }


    if (unlikely(epoll_ctl(loop->epoll_fd, EPOLL_CTL_DEL, eventfd, NULL)==-1)) {
        plog_error("epoll_ctl DEL error: %d %s", errno, strerror(errno));
    }

    --pobj_internal_events_last_event;

    loop->ref_count--;
    return true;
}

static void pobj_timer_internal_update(pobj_loop* loop, const puint32 epoll_events)
{
    if (unlikely(!loop)) {
        plog_error("pobj_timer_internal_update() нет объекта");
        return;
    }

    if (unlikely(!epoll_events)) {
        plog_error("pobj_timer_internal_update() нет события");
        return;
    }

    puint64 exp;
    ssize_t s = read(loop->internal_timer.timer.descriptor, &exp, sizeof(puint64));
    if (s != sizeof(puint64)) {
        plog_error("pobj_timer_internal_update() ошибка чтения таймера");
        return;
    }

    loop->internal_timer.callback(loop, epoll_events);
}

bool pobj_internal_timer_start(pobj_loop *loop, const puint8 repeating, const struct timespec time, void (*callback)())
{
    if (unlikely(!loop)) {
        plog_error("callback() нет объекта");
        return false;
    }

    if (unlikely(!callback)) {
        plog_error("callback() нет обратной функции");
        return false;
    }

    loop->internal_timer.timer.repeating = repeating;
    loop->internal_timer.timer.time_ms = time;

    if (!loop->internal_timer.timer.descriptor) {
        if (!pobj_timer_create(&loop->internal_timer.timer)) {
            return false;
        }
    }

    if (!pobj_timer_start(&loop->internal_timer.timer)) {
        return false;
    }

    pobj_register_event(loop, pobj_timer_internal_update, loop->internal_timer.timer.descriptor, POBJIN);
    loop->internal_timer.callback = callback;

    return true;
}

void pobj_internal_timer_stop(pobj_loop *loop)
{
    if (unlikely(!loop)) {
        plog_error("pobj_internal_timer_stop() нет объекта");
        return;
    }

    if (loop->internal_timer.timer.descriptor) {
        pobj_unregister_event(loop, loop->internal_timer.timer.descriptor);
        pobj_timer_destroy(&loop->internal_timer.timer);
    }
}


void pobj_destroy(pobj_loop **loop)
{
    if (unlikely(!(*loop))) {
        plog_error("pobj_destroy() нет объекта");
        return;
    }

    close((*loop)->epoll_fd);
    pfree(loop);

    pfree(&pobj_internal_events);
}
