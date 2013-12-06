#ifndef CORE_LOG_H_
#define CORE_LOG_H_

// Типы сообщений
enum msg_item_type
{
    PUR_MSG_INFO=0, // Информационное
    PUR_MSG_ERR,    // Ошибка
    PUR_MSG_WARN,   // Предупреждение
    PUR_MSG_DBG    // Отладка
};

// Написать сообщение
void pcore_log(int, const char *, const char *, ...);
 
#ifndef MODULE_NAME
#define MODULE_NAME 'unknown'
#endif

#define plog_info( ... ) pcore_log( PUR_MSG_INFO, MODULE_NAME, __VA_ARGS__ )
#define plog_error( ... ) pcore_log( PUR_MSG_ERR, MODULE_NAME, __VA_ARGS__ )
#define plog_warn( ... ) pcore_log( PUR_MSG_WARN, MODULE_NAME, __VA_ARGS__ )
#define plog_dbg( ... ) pcore_log( PUR_MSG_DBG, MODULE_NAME, __VA_ARGS__ )

#endif
