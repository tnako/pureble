#ifdef MODULE_NAME
#undef MODULE_NAME
#endif

#define MODULE_NAME "papp/help"

#include "plog/log.h"
#include "papp/help.h"
#include "papp/args.h"
#include "pcore/internal.h"


char *header = NULL;
char *footer = NULL;

void papp_help_set_header(const char *string)
{
    if (unlikely(!string)) {
        plog_warn("Невозможно подготовить шапку справки");
        return;
    }

    if (unlikely(header)) {
        pfree(&header);
    }

    header = strdup(string);
}


void papp_help_set_footer(const char *string)
{
    if (unlikely(!string)) {
        plog_warn("Невозможно подготовить низ справки");
        return;
    }

    if (unlikely(footer)) {
        pfree(&footer);
    }

    footer = strdup(string);
}

static void papp_help_show_onearg(const puint32 UNUSED(value), void *data)
{
    if (unlikely(!data)) {
        plog_warn("Нету объекта перечисления");
        return;
    }

    one_arg *argument = data;
    fprintf(stdout, "\t\t-%c\t--%15s\t%10s\t%s\n", argument->short_name, argument->long_name, (argument->has_arg ? "[значение]" : ""), argument->description);
}

void papp_help_show()
{
    if (likely(header)) {
        fwrite(header, strlen(header), 1, stdout);
    }

    papp_args_list(papp_help_show_onearg);

    if (likely(footer)) {
        fwrite(footer, strlen(footer), 1, stdout);
    }
}
