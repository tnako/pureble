#ifndef CORE_INTERNAL_H_
#define CORE_INTERNAL_H_

#include <string.h>
#include <malloc.h>

#include "plog/log.h"


#define _GNU_SOURCE

#ifdef __LP64__
#define MEM_GUARD 64
#define _FILE_OFFSET_BITS 64
#else
#define MEM_GUARD 32
#define _FILE_OFFSET_BITS 32
#endif

#ifdef __GNUC__
#  define UNUSED(x) UNUSED_ ## x __attribute__((__unused__))
#else
#  define UNUSED(x) UNUSED_ ## x
#endif

#define pmalloc(a) pmalloc_check(__PRETTY_FUNCTION__, a)
#define pfree(a) pfree_check(__PRETTY_FUNCTION__, (void **)a)

#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

static inline void* pmalloc_check(const char *func, size_t __size)
{
    void *adr = memalign(MEM_GUARD, (__size)+MEM_GUARD);
    if (unlikely(!adr)) {
        plog_error("%s(): Не удалось выделить память", func);
        return NULL;
    }

    memset(adr, 0x0, (__size)+MEM_GUARD);

    return adr;
}

static inline void pfree_check(const char *func, void **adr)
{
    if (unlikely(!(*adr))) {
        plog_warn("%s(): Попытка двойной очистки", func);
        return;
    }

    free((*adr));
    (*adr) = NULL;
}


#endif
