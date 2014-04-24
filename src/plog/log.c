#ifdef MODULE_NAME
#undef MODULE_NAME
#endif

#define MODULE_NAME "pcore/logging"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <syslog.h>


#include "plog/log.h"
#include "pcore/internal.h"
#include "pcore/types.h"

#define DATETIME_FORMAT "%Y-%m-%d %H:%M:%S"
#define DATETIME_SIZE 20 // sizeof("2013-03-21 11:41:59")


static const char msg_type[5][8] = { "", "debug", "info", "warning", "error"  };

char dateTime[DATETIME_SIZE];
struct timeval rawtime;
struct tm timeinfo;
bool syslog_enabled = false;
static int LogMask = 0;


void pcore_log(int type, const char *module, const char *format, ... )
{
    if (LOG_MASK(type) & LogMask) {
        return;
    }

    if (unlikely(!module)) {
        fprintf(stderr, "%s(): Нет имени модуля!", __PRETTY_FUNCTION__);
        return;
    }

    if (unlikely(!format)) {
        fprintf(stderr, "%s(): Нет текста для отображения!", __PRETTY_FUNCTION__);
        return;
    }

    va_list args;

    if (syslog_enabled) {
        pint32 log_level;

        switch(type) {
        case PUR_MSG_INFO:
            log_level = LOG_INFO;
            break;
        case PUR_MSG_ERR:
            log_level = LOG_ERR;
            break;
        case PUR_MSG_WARN:
            log_level = LOG_WARNING;
            break;
        case PUR_MSG_DBG:
        default:
            log_level = LOG_DEBUG;
        }

        char new_format[4096];
        snprintf(new_format, 4095, "%s | %s", module, format);

        va_start(args, format);
        vsyslog(log_level, new_format, args);
        va_end(args);
    } else {
        FILE *stream = stdout;

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
}


void pcore_log_syslog_init(const char *service)
{
    if (unlikely(!service)) {
        return;
    }

    openlog(service, LOG_PID|LOG_CONS|LOG_NDELAY, LOG_USER);
    syslog_enabled = true;
}


void pcore_log_syslog_close()
{
    closelog();
    syslog_enabled = false;
}


void pcore_log_mask_set(int new_mask)
{
    if (new_mask > 0) {
        LogMask = LOG_UPTO(new_mask);
    } else {
       pcore_log_mask_reset();
    }
}


void pcore_log_mask_reset()
{
    LogMask = 0;
}
