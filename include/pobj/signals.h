#ifndef OBJ_SIGNALS_H_
#define OBJ_SIGNALS_H_

#include "pcore/types.h"
#include "pobj/object.h"

#include <signal.h>



bool pobj_signals_add(pobj_loop *loop, pint8 signal, void (*callback)());
bool pobj_signals_del(pobj_loop *loop, pint8 signal);

bool pobj_signals_start(pobj_loop *loop);

// ToDo: unregister events

#endif
