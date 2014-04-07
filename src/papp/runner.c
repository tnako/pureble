#ifdef MODULE_NAME
#undef MODULE_NAME
#endif

#define MODULE_NAME "papp/runner"


#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>


#include "plog/log.h"
#include "papp/runner.h"
#include "pcore/internal.h"


void papp_runner_daemonize()
{
    pid_t pid, sid;

    pid = fork();
    if (unlikely(pid < 0)) {
        plog_error("Не удалось уйти в fork()");
        exit(EXIT_FAILURE);
    }

    // Выход из родительского процесса
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    sid = setsid();
    if (unlikely(sid < 0)) {
        plog_error("Не удалось сменить setsid()");
        exit(EXIT_FAILURE);
    }

    umask(0);
}

void papp_run(pint8 daemonize, char *pid_file, void (*app_main)())
{

    if (unlikely(!app_main)) {
        plog_error("Нет основной функции для выполнения!");
        return;
    }

    if (pid_file) {
        plog_error("ToDo: Создание PID");
    }

    if (daemonize) {
        papp_runner_daemonize();
    }

    app_main();

    //ToDo: unlink pid
}
