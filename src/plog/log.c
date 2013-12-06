#ifdef MODULE_NAME
#undef MODULE_NAME
#endif
#define MODULE_NAME "pcore/logging"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

//#include "main.h"


#define DATETIME_FORMAT "%Y-%m-%d %H:%M:%S"
#define DATETIME_SIZE 20 // sizeof("2013-03-21 11:41:59")


static const char msg_type[4][8] = { "info", "error", "warning", "debug" };

char dateTime[DATETIME_SIZE];
struct timeval rawtime;
struct tm timeinfo;

    
void pcore_log(int type, const char *module, const char *format, ... )
{
    if (!module) {
        fprintf(stderr, "%s(): Нет имени модуля!", __PRETTY_FUNCTION__);
        return;
    }

    if (!format) {
        fprintf(stderr, "%s(): Нет текста для отображения!", __PRETTY_FUNCTION__);
        return;
    }

    va_list args;

    FILE *stream = stdout;
/*    
 * ToDo: log to file
    if (type == MSG_ERR) {
      stream = stderr;
    } else {
      stream = stdout;
    }
*/    

//    Получаем текущее время
    gettimeofday(&rawtime, NULL);
    // ToDo: свою реализацию date_time, так как localtime_r блокирующий в libc на весь userspace
    // https://github.com/schwern/y2038/blob/master/time64.c

    localtime_r(&rawtime.tv_sec, &timeinfo);
    strftime(dateTime, DATETIME_SIZE , DATETIME_FORMAT, &timeinfo);

    flockfile(stream);
    fprintf(stream, "%s.%03u | %*s%*s | %*s%*s: ", dateTime,
            (unsigned int)rawtime.tv_usec / 1000,
            (int)(10+strlen(module)/2),module,(int)(10-strlen(module)/2),"",
            (int)(4+strlen(msg_type[type])/2),msg_type[type],(int)(4-strlen(msg_type[type])/2),"");
    
    va_start(args, format);
    vfprintf(stream, format, args);
    va_end(args);
    
    putc_unlocked('\n', stream);
    funlockfile(stream);
}
