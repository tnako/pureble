#ifndef SQL_MARIADB_H_
#define SQL_MARIADB_H_

#include "pcore/types.h"


#include <mysql.h>


MYSQL *psql_mariadb_start(const char *address, const char *user, const char *password, const char *database);
void psql_mariadb_stop(MYSQL **connection);

MYSQL_STMT *psql_mariadb_stm_init(MYSQL *connection, MYSQL_BIND *params, MYSQL_BIND *results, const char *query);
bool psql_mariadb_stm_execute(MYSQL_STMT *stmt);
bool psql_mariadb_stm_fetch(MYSQL_STMT *stmt);


#endif
