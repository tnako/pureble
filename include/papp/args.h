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

typedef struct {
    const char *long_name;
    const char *description;
    int short_name;
    int has_arg;
    void (*ret_func)(const char*);
} one_arg;


// Очистка
void papp_args_destroy();

// Добавление нового элемента для парсинга аргументов
bool papp_args_add(char *long_name,
                   const char short_name,
                   const int has_arg,
                   void (*ret_func)(const char*),
                   char *description);

// Обработка списка аргументов
bool papp_args_check(const int argc, char* const argv[], void (*default_func)(const char*));

// Обработка каждого элемента
void papp_args_list(void (*ret_func)(const puint32, void*));


// ToDo:
// void *remove(const char short_name);


#endif
