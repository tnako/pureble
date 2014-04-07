#ifndef APP_RUNNER_H_
#define APP_RUNNER_H_

#include "pcore/types.h"

void papp_run(pint8 daemonize, char *pid_file, void (*app_main)());


#endif
