#ifdef MODULE_NAME
#undef MODULE_NAME
#endif
#define MODULE_NAME "papp/arguments"

#include "plog/log.h"
#include "papp/args.h"
#include "pcore/hash.h"
#include "pcore/internal.h"

#include <getopt.h>


typedef struct {
    char *long_name;
    char short_name;
    int has_arg;
    void (*ret_func)(const char*);
} one_arg;

phash_pool args_pool = NULL;

puint16 short_names_size = 0;
struct option *long_names = NULL;
char *short_names = NULL;


static void papp_args_fill(const puint32 value, void* const data)
{
    if (!data || !short_names || !long_names || !value) {
        plog_warn("Невозможно подготовить getopt()");
        return;
    }

    //strncat(short_names, data->short_name, 1);

}


bool papp_args_check(const int argc, char* const argv[], void (*default_func)(const char*))
{
    if (argc <= 1 || !argv) {
        plog_error("Нету допольнительных аргументов");
        return false;
    }

    puint32 hash_size = pcore_hash_size(args_pool);

    if (hash_size == 0) {
        plog_warn("Нету аргументов для проверки!");
        return false;
    }

    long_names = pmalloc(sizeof(struct option) * (hash_size + 1));
    short_names = pmalloc(short_names_size);

    //pint32 option_index = 0;

    pcore_hash_foreach(args_pool, papp_args_fill);

    default_func("test");

    pfree((void **)&short_names);
    pfree((void **)&long_names);

    return true;
}

bool papp_args_add(const char* long_name,
                   const char short_name,
                   const puint8 has_arg,
                   void (*ret_func)(const char*))
{
    if (!long_name) {
        plog_error("Нельзя добавить обработчик длинного параметра");
        return false;
    }

    if (short_name < 65 ||
            short_name > 122 ||
            (short_name > 90 && short_name < 97)) {
        plog_error("Нельзя добавить обработчик короткого параметра");
        return false;
    }

    if (has_arg > PARGS_OPTIONAL_ARGUMENT) {
        plog_error("Неверное требование аргумеента, код: %u", has_arg);
        return false;
    }

    if (!ret_func) {
        plog_error("Нету callback функции для параметра: '%s' / '%c'", long_name, short_name);
        return false;
    }

    if (!args_pool) {
        args_pool = pcore_hash_init(PHASH_FAST_INSERT);
    }

    pcore_hash_insert(args_pool, short_name, ret_func);

    if (has_arg == PARGS_NO_ARGUMENT) {
        ++short_names_size;
    } else {
        short_names_size += 2;
    }

    return true;
}
/*
static void free_args_object(phash_object obj)
{
    if (!obj) {
        plog_warn("Нету phash_object");
        return;
    }

    pcore_hash_deleteObject(&obj);
}
*/

void papp_args_destroy()
{
    if (!args_pool) {
        plog_warn("Невозможно очистить пул аргументов!");
        return;
    }

    //pcore_hash_foreach(args_pool, free_args_object);
    pcore_hash_done(&args_pool);
}
