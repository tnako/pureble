#ifndef CORE_INTERNAL_H_
#define CORE_INTERNAL_H_

#ifdef __GNUC__
#  define UNUSED(x) UNUSED_ ## x __attribute__((__unused__))
#else
#  define UNUSED(x) UNUSED_ ## x
#endif

#include <string.h>

#include "plog/log.h"

#define pmalloc(a) pmalloc_check(__PRETTY_FUNCTION__, a)
#define pfree(a) pfree_check(__PRETTY_FUNCTION__, a)

static inline void* pmalloc_check(const char *func, size_t __size)
{
    void *adr = malloc(__size);
    if (!adr) {
        plog_error("%s(): Не удалось выделить память", func);
        return NULL;
    }

    memset(adr, 0x0, __size);

    return adr;
}

static inline void pfree_check(const char *func, void **adr)
{
    if (!(*adr)) {
        plog_warn("%s(): Попытка двойной очистки", func);
        return;
    }

    free((*adr));
    (*adr) = NULL;
}


#endif
