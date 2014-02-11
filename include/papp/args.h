#ifndef APP_ARGS_H_
#define APP_ARGS_H_

#include <stdbool.h>
#include <stdlib.h>

#include "pcore/types.h"

enum {
    PARGS_NO_ARGUMENT = 0,
    PARGS_REQUIRED_ARGUMENT,
    PARGS_OPTIONAL_ARGUMENT
};

// Очистка
void papp_args_destroy();

// Добавление нового элемента для парсинга аргументов
bool papp_args_add(char *long_name,
                   const char short_name,
                   const int has_arg,
                   void (*ret_func)(const char*));

// Обработка списка аргументов
bool papp_args_check(const int argc, char* const argv[], void (*default_func)(const char*));


// ToDo:
// void *remove(const char short_name);
// void *list();


#endif
