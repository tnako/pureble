#ifdef MODULE_NAME
#undef MODULE_NAME
#endif

#define MODULE_NAME "psql/mariadb"

#include "psql/mariadb.h"
#include "plog/log.h"
#include "pcore/internal.h"



MYSQL *psql_mariadb_start(const char *address, const char *user, const char *password, const char *database)
{
    if (unlikely(!address || !user || !password || !database)) {
        plog_error("psql_mariadb_start() Нет данных для подключени");
        return NULL;
    }

    int err = mysql_library_init(-1, NULL, NULL);
    if (unlikely(err)) {
        plog_error("psql_mariadb_start() mysql_library_init() вернул код ошибки: %d", err);
        return NULL;
    }

    MYSQL *mariadb;

    mariadb = mysql_init(NULL);
    mariadb->reconnect = true;

    if (unlikely(mysql_real_connect(mariadb, address, user, password, database, 0, NULL, 0) == NULL)) {
        plog_error("psql_mariadb_start() невозможно подключиться к БД | %s", mysql_error(mariadb));
        mysql_library_end();
        return NULL;
    }

    if (unlikely(mysql_autocommit(mariadb, 0))) {
        plog_error("psql_mariadb_start() невозможно отключить mysql_autocommit");
        mysql_close(mariadb);
        mysql_library_end();
        return NULL;
    }

    return mariadb;
}



void psql_mariadb_stop(MYSQL **connection)
{
    if (unlikely(!(*connection))) {
        plog_error("psql_mariadb_stop() Нету MYSQL");
        return;
    }

    mysql_close(*connection);
    mysql_library_end();
    *connection = NULL;
}


MYSQL_STMT *psql_mariadb_stm_init(MYSQL *connection, MYSQL_BIND *params, MYSQL_BIND *results, const char *query)
{
    if (unlikely(!connection)) {
        plog_error("psql_mariadb_stm_init() Нету MYSQL connection");
        return NULL;
    }

    if (unlikely(!params)) {
        plog_error("psql_mariadb_stm_init() Нету MYSQL_BIND params");
        return NULL;
    }

    if (unlikely(!results)) {
        plog_error("psql_mariadb_stm_init() Нету MYSQL_BIND results");
        return NULL;
    }

    if (unlikely(!query)) {
        plog_error("psql_mariadb_stm_init() Нету query");
        return NULL;
    }

    MYSQL_STMT *stmt;

    stmt = mysql_stmt_init(connection);
    if (!stmt) {
        plog_error("psql_mariadb_stm_init() не могу mysql_stmt_init");
        return NULL;
    }

    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        plog_error("psql_mariadb_stm_init() Не могу mysql_stmt_prepare: %s", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return NULL;
    }

    if (mysql_stmt_bind_param(stmt, params)) {
        plog_error("psql_mariadb_stm_init() Не могу mysql_stmt_bind_param: %s", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return NULL;
    }

    if (mysql_stmt_bind_result(stmt, results) != 0) {
        plog_error("psql_mariadb_stm_init() Не могу mysql_stmt_bind_result: %s", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return NULL;
    }

    return stmt;
}

bool psql_mariadb_stm_execute(MYSQL_STMT *stmt)
{
    if (unlikely(!stmt)) {
        plog_error("psql_mariadb_stm_execute() Нету MYSQL_STMT");
        return false;
    }

    if (unlikely(mysql_stmt_execute(stmt))) {
        plog_error("psql_mariadb_stm_execute() Не могу mysql_stmt_execute: %s", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }

    return true;
}

bool psql_mariadb_stm_fetch(MYSQL_STMT *stmt)
{
    if (unlikely(!stmt)) {
        plog_error("psql_mariadb_stm_fetch() Нету MYSQL_STMT");
        return false;
    }

    if(mysql_stmt_fetch(stmt) == 0) {
        return true;
    }

    mysql_stmt_free_result(stmt);
    return false;
}
