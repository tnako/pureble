#ifndef OBJ_TIMER_H_
#define OBJ_TIMER_H_

#include "pcore/types.h"

#include <sys/timerfd.h>

typedef struct {
    bool repeating;
    bool done;
    pint32 descriptor;
    struct timespec time_ms;
} pobj_timer;

bool pobj_timer_create(pobj_timer*);

bool pobj_timer_start(pobj_timer*);

void pobj_timer_destroy(pobj_timer*);


#endif
