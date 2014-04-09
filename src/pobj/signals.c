#ifdef MODULE_NAME
#undef MODULE_NAME
#endif

#define MODULE_NAME "pobj/signals"


#include "plog/log.h"
#include "pobj/signals.h"
#include "pcore/internal.h"

#include <sys/signalfd.h>
#include <errno.h>
#include <unistd.h>



bool pobj_signals_add(pobj_loop *loop, puint8 signal, void (*callback)())
{
    if (unlikely(!loop)) {
        plog_error("pobj_signals_add() нет объекта");
        return false;
    }

    if (unlikely(!callback)) {
        plog_error("pobj_signals_add() нет возвращаемой функции");
        return false;
    }

    loop->signals_callback[signal] = callback;
    return true;
}

bool pobj_signals_del(pobj_loop* loop, puint8 signal)
{
    if (unlikely(!loop)) {
        plog_error("pobj_signals_del() нет объекта");
        return false;
    }

    loop->signals_callback[signal] = NULL;
    return true;
}

static void pobj_signals_internal_update(pobj_loop* loop, const puint32 epoll_events)
{
    if (epoll_events & POBJIN) {
        for (;;) {
            struct signalfd_siginfo fdsi;
            pint32 s = read(loop->signals_fd, &fdsi, sizeof(struct signalfd_siginfo));
            if (s < (pint32)sizeof(struct signalfd_siginfo)) {
                return;
            }

            if (loop->signals_callback[fdsi.ssi_signo]) {
                (*loop->signals_callback[fdsi.ssi_signo])();
            } else {
                plog_error("pobj_signals_internal_update() прочитан неожиданный signal");
            }
        }
    } else {
        return;
    }
}

bool pobj_signals_start(pobj_loop* loop)
{
    if (unlikely(!loop)) {
        plog_error("pobj_signals_start() нет объекта");
        return false;
    }

    if (unlikely(loop->signals_fd)) {
        pobj_signals_stop(loop);
    }

    sigset_t sigmask;
    sigemptyset(&sigmask);

    puint8 found = 0;
    for (int i = 0; i < _NSIG; i++) {
        if (loop->signals_callback[i]) {
            sigaddset(&sigmask, i);
            ++found;
        }
    }

    if (unlikely(found == 0)) {
        plog_error("pobj_signals_start() нету сигналов для обработки");
        return false;
    }

    if (unlikely(sigprocmask(SIG_BLOCK, &sigmask, NULL) < 0)) {
        plog_error("Неудалось заблокировать POSIX сигнал: '%s'", strerror(errno));
        return false;
    }

    int signal_fd;
    if (unlikely((signal_fd = signalfd(-1, &sigmask, SFD_NONBLOCK)) < 0)) {
        plog_error("Неудалось настроить POSIX сигнал: '%s'", strerror(errno));
        return false;
    }


    if (unlikely(!pobj_register_event(loop, pobj_signals_internal_update, signal_fd, POBJIN))) {
        plog_error("pobj_signals_start() неудалось зарегистрировать событие");
        return false;
    }

    loop->signals_fd = signal_fd;

    return true;
}

void pobj_signals_stop(pobj_loop *loop)
{
    if (unlikely(!loop)) {
        plog_error("pobj_signals_stop() нет объекта");
        return;
    }

    if (!loop->signals_fd) {
        return;
    }

    pobj_unregister_event(loop, loop->signals_fd);
    close(loop->signals_fd);
    loop->signals_fd = 0;
}
