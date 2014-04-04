#ifdef MODULE_NAME
#undef MODULE_NAME
#endif

#define MODULE_NAME "papp/arguments"

#include "plog/log.h"
#include "papp/args.h"
#include "pcore/hash.h"
#include "pcore/internal.h"

#include <getopt.h>


phash_pool args_pool = NULL;

puint16 short_names_size = 0;
struct option *long_names_array = NULL;
char *short_names = NULL;
puint32 short_options_position;
puint32 long_options_position;


static void papp_args_fill(const puint32 value, void* const data)
{
    if (!data || !short_names || !long_names_array || !value) {
        plog_warn("Невозможно подготовить getopt()");
        return;
    }

    if (short_options_position == short_names_size) {
        plog_warn("Список коротких аргументов меньше размера. WTF!");
        return;
    }

    const one_arg *parameter = data;

    short_names[short_options_position++] = parameter->short_name;

    if (parameter->has_arg) {
        short_names[short_options_position++] = ':';
    }

    struct option *long_options = long_names_array + long_options_position++;
    long_options->name = parameter->long_name;
    long_options->has_arg = parameter->has_arg;
    long_options->val = parameter->short_name;
}

bool papp_args_check(const int argc, char* const argv[], void (*default_func)(const char*))
{
    if (unlikely(argc <= 1 || !argv || !default_func)) {
        plog_warn("Нету дополнительных аргументов");
        return false;
    }

    puint32 hash_size = pcore_hash_size(args_pool);

    if (unlikely(hash_size == 0)) {
        plog_warn("Нету аргументов для проверки!");
        return false;
    }

    long_names_array = pmalloc(sizeof(struct option) * (hash_size + 1));
    short_names = pmalloc(short_names_size + 1);

    short_options_position = 0;
    long_options_position = 0;
    pcore_hash_foreach(args_pool, papp_args_fill);
    short_names[short_options_position] = 0;

    plog_dbg("getopt_long(): '%s'", short_names);
    while (1) {
        int option_index = 0;
        int opt_res = getopt_long(argc, argv, short_names, long_names_array, &option_index);

        if (opt_res == -1) {
            break;
        } else if (opt_res == '?') {
            /* getopt_long already printed an error message. */
            continue;
        }

        const one_arg *parameter = pcore_hash_search(args_pool, opt_res);

        if (parameter == NULL) {
            char tmp[2] = { 0 };
            tmp[0] = opt_res;
            default_func(tmp);
        } else {
            parameter->ret_func(optarg);
        }
    }

    pfree(&short_names);
    pfree(&long_names_array);

    return true;
}

bool papp_args_add(char *long_name,
                   const char short_name,
                   const int has_arg,
                   void (*ret_func)(const char*),
                   char *description)
{
    if (unlikely(!long_name)) {
        plog_error("Нельзя добавить обработчик длинного параметра");
        return false;
    }

    if (unlikely(short_name < 65 ||
            short_name > 122 ||
            (short_name > 90 && short_name < 97))) {
        plog_error("Нельзя добавить обработчик короткого параметра");
        return false;
    }

    if (unlikely(has_arg > PARGS_OPTIONAL_ARGUMENT)) {
        plog_error("Неверное требование аргумеента, код: %u", has_arg);
        return false;
    }

    if (unlikely(!ret_func)) {
        plog_error("Нету callback функции для параметра: '%s' / '%c'", long_name, short_name);
        return false;
    }

    if (!args_pool) {
        args_pool = pcore_hash_init(PHASH_FAST_INSERT);
    }

    one_arg *parameter = pmalloc(sizeof(one_arg));
    parameter->has_arg = has_arg;
    parameter->long_name = long_name;
    parameter->description = description;
    parameter->ret_func = ret_func;
    parameter->short_name = short_name;

    pcore_hash_insert(args_pool, short_name, parameter);

    if (has_arg == PARGS_NO_ARGUMENT) {
        ++short_names_size;
    } else {
        short_names_size += 2;
    }

    return true;
}

static void papp_args_free(const puint32 UNUSED(value), void *data)
{
    if (unlikely(!data)) {
        plog_warn("Нету объекта удаления");
        return;
    }

    pfree(&data);
}

void papp_args_destroy()
{
    if (unlikely(!args_pool)) {
        plog_warn("Невозможно очистить пул аргументов!");
        return;
    }

    pcore_hash_foreach(args_pool, papp_args_free);
    pcore_hash_done(&args_pool);
    args_pool = NULL;
}

void papp_args_list(void (*ret_func)(const puint32, void*))
{
    if (unlikely(!ret_func)) {
        plog_warn("Нету функции-обработчика для перебора всех элементов");
        return;
    }

    if (unlikely(!args_pool)) {
        plog_warn("Невозможно перебрать пул аргументов!");
        return;
    }

    pcore_hash_foreach(args_pool, ret_func);
}
