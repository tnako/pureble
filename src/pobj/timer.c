#ifdef MODULE_NAME
#undef MODULE_NAME
#endif

#define MODULE_NAME "pobj/timer"

#include "pobj/timer.h"
#include "plog/log.h"
#include "pcore/internal.h"

#include <unistd.h>
#include <errno.h>

bool pobj_timer_create(pobj_timer *timer)
{
    if (unlikely(!timer)) {
        plog_error("pobj_timer_create() нету pobj_timer");
        return false;
    }

    timer->descriptor = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (unlikely(!timer->descriptor)) {
        plog_error("timerfd_create: %d %s", errno, strerror(errno));
        return false;
    }

    return true;
}

bool pobj_timer_start(pobj_timer *timer)
{
    if (unlikely(!timer)) {
        plog_error("pobj_timer_start() нету pobj_timer");
        return false;
    }

    if (unlikely(!timer->descriptor)) {
        if (unlikely(!pobj_timer_create(timer))) {
            return false;
        }
    }

    if (timer->done == 1) {
        timer->done = 0;
    }

    struct itimerspec new_timeout;
    new_timeout.it_value = timer->time_ms;

    if (timer->repeating) {
        new_timeout.it_interval = timer->time_ms;
    } else {
        new_timeout.it_interval.tv_sec = new_timeout.it_interval.tv_nsec = 0;
    }

    int res = timerfd_settime(timer->descriptor, 0, &new_timeout, 0);
    if(unlikely(res < 0)) {
        plog_error("timerfd_settime: %d %s", errno, strerror(errno));
        close(timer->descriptor);
        timer->done = 1;
        return false;
    }

    return true;
}

void pobj_timer_destroy(pobj_timer *timer)
{
    if (unlikely(!timer)) {
        plog_error("pobj_timer_destroy() нету pobj_timer");
        return;
    }

    close(timer->descriptor);
    timer->descriptor = 0;
    timer->done = 1;
}
