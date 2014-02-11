#include <pureble.h>

void default_func(const char* string)
{
    plog_info("Default arg: %s", string);
}

void print_time(const char* string)
{
    plog_info("Time arg: %s", string);
}

void print_date(const char* string)
{
    plog_info("Date arg: %s", string);
}

int main(int argc, char *argv[])
{
    plog_info("Start tests");

    plog_info("Normal test");
    papp_args_add("time", 't', true, print_time);
    papp_args_add("date", 'd', true, print_date);
    papp_args_check(argc, argv, default_func);
    papp_args_destroy();

    plog_info("papp_args_destroy()");
    papp_args_destroy();

    plog_info("papp_args_check() + papp_args_destroy()");
    papp_args_check(argc, argv, default_func);
    papp_args_destroy();

    plog_info("papp_args_add() + papp_args_check() + papp_args_destroy()");
    papp_args_add("time", 't', true, print_time);
    papp_args_check(argc, argv, default_func);
    papp_args_destroy();


    return 0;
}

