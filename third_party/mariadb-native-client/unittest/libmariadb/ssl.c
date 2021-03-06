/************************************************************************************
  Copyright (C) 2012 Monty Program AB

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.

  You should have received a copy of the GNU Library General Public
  License along with this library; if not see <http://www.gnu.org/licenses>
  or write to the Free Software Foundation, Inc.,
  51 Franklin St., Fifth Floor, Boston, MA 02110, USA
 *************************************************************************************/

#include "my_test.h"
#include <my_pthread.h>

static int skip_ssl= 1;

#ifndef WIN32
pthread_mutex_t LOCK_test;
#endif

int check_skip_ssl()
{
#ifndef HAVE_OPENSSL
  diag("client library built without OpenSSL support -> skip");
  return 1;
#endif
  if (skip_ssl)
  {
    diag("server doesn't support SSL -> skip");
    return 1;
  }
  return 0;
}

static int test_ssl(MYSQL *mysql)
{
  int rc;
  MYSQL_RES *res;
  MYSQL_ROW row;

  rc= mysql_query(mysql, "SELECT @@have_ssl");
  check_mysql_rc(rc, mysql);

  res= mysql_store_result(mysql);
  FAIL_IF(!res, mysql_error(mysql));

  if ((row= mysql_fetch_row(res)))
  {
    if (!strcmp(row[0], "YES"))
      skip_ssl= 0;
    diag("SSL: %s", row[0]);
  }
  mysql_free_result(res);

  return OK;
}

static int test_ssl_cipher(MYSQL *unused)
{
  MYSQL *my;
  char  *cipher;
  
  if (check_skip_ssl())
    return SKIP;

  my= mysql_init(NULL);
  FAIL_IF(!my, "mysql_init() failed");

  mysql_ssl_set(my,0, 0, "C:/work/mariadb-native-client/unittest/libmariadb/certs/ca.pem", 0, 0);

  FAIL_IF(!mysql_real_connect(my, hostname, username, password, schema,
                         port, socketname, 0), mysql_error(my));

  cipher= (char *)mysql_get_ssl_cipher(my);
  FAIL_IF(strcmp(cipher, "DHE-RSA-AES256-SHA") != 0, "Cipher != DHE-RSA-AES256-SHA");
  mysql_close(my);
  return OK;
}

static int test_multi_ssl_connections(MYSQL *unused)
{
  MYSQL *mysql[50], *my;
  char *cipher;
  int i, rc;
  int old_connections= 0, new_connections= 0;
  MYSQL_RES *res;
  MYSQL_ROW row;

  if (check_skip_ssl())
    return SKIP;

  diag("Test doesn't work with yassl");
  return SKIP;

  my= mysql_init(NULL);
  FAIL_IF(!my,"mysql_init() failed");
  FAIL_IF(!mysql_real_connect(my, hostname, username, password, schema,
           port, socketname, 0), mysql_error(my));

  rc= mysql_query(my, "SHOW STATUS LIKE 'Ssl_accepts'");
  check_mysql_rc(rc, my);

  res= mysql_store_result(my);
  if ((row= mysql_fetch_row(res)))
    old_connections= atoi(row[1]);
  mysql_free_result(res);

  for (i=0; i < 50; i++)
  {
    mysql[i]= mysql_init(NULL);
    FAIL_IF(!mysql[i],"mysql_init() failed");

    mysql_ssl_set(mysql[i], 0, 0, "C:/work/mariadb-native-client/unittest/libmariadb/certs/ca.pem", 0, 0);

    mysql_real_connect(mysql[i], hostname, username, password, schema,
                         port, socketname, 0);
    if (mysql_errno(mysql[i]))
    {
      diag("loop: %d error: %d %s", i, mysql_errno(mysql[i]), mysql_error(mysql[i]));
      return FAIL;
    }

    cipher= (char *)mysql_get_ssl_cipher(mysql[i]);
    FAIL_IF(strcmp(cipher, "DHE-RSA-AES256-SHA") != 0, "Cipher != DHE-RSA-AES256-SHA");
  }
  for (i=0; i < 50; i++)
    mysql_close(mysql[i]);

  rc= mysql_query(my, "SHOW STATUS LIKE 'Ssl_accepts'");
  check_mysql_rc(rc, my);

  res= mysql_store_result(my);
  if ((row= mysql_fetch_row(res)))
    new_connections= atoi(row[1]);
  mysql_free_result(res);

  mysql_close(my);

  diag("%d SSL connections processed", new_connections - old_connections);
  FAIL_IF(new_connections - old_connections < 50, "new_connections should be at least old_connections + 50");
  return OK;
}

#ifndef WIN32
static void ssl_thread(void)
{
  MYSQL *mysql;

  mysql_thread_init();
  
  if (!(mysql= mysql_init(NULL)))
  {  
    mysql_thread_end();
    pthread_exit(NULL);
  }
  mysql_ssl_set(mysql, 0, 0, "C:/work/mariadb-native-client/unittest/libmariadb/certs/ca.pem", 0, 0);

  if(!mysql_real_connect(mysql, hostname, username, password, schema,
          port, socketname, 0))
  {
    diag(">Error: %s", mysql_error(mysql));
    mysql_close(mysql);
    mysql_thread_end();
    pthread_exit(NULL);
  }

  pthread_mutex_lock(&LOCK_test);
  mysql_query(mysql, "UPDATE ssltest SET a=a+1");
  pthread_mutex_unlock(&LOCK_test);
  mysql_close(mysql);
  mysql_thread_end();
  pthread_exit(0);
}

static int test_ssl_threads(MYSQL *mysql)
{
  int i, rc;
  pthread_t thread[50];
  MYSQL_RES *res;
  MYSQL_ROW row;
  
  if (check_skip_ssl())
    return SKIP;

  rc= mysql_query(mysql, "DROP TABLE IF exists ssltest");
  check_mysql_rc(rc, mysql);
  rc= mysql_query(mysql, "CREATE TABLE ssltest (a int)");
  check_mysql_rc(rc, mysql);
  rc= mysql_query(mysql, "INSERT into ssltest VALUES (0)");
  check_mysql_rc(rc, mysql);

  pthread_mutex_init(&LOCK_test, NULL);

  for (i=0; i < 50; i++)
    pthread_create(&thread[i], NULL, (void *)&ssl_thread, NULL);
  for (i=0; i < 50; i++)
    pthread_join(thread[i], NULL);

  pthread_mutex_destroy(&LOCK_test);

  rc= mysql_query(mysql, "SELECT a FROM ssltest");
  check_mysql_rc(rc, mysql);
  res= mysql_store_result(mysql);
  row= mysql_fetch_row(res);
  diag("Found: %s", row[0]);
  FAIL_IF(strcmp(row[0], "50") != 0, "Expected 50");
  mysql_free_result(res);
  return OK;
}
#endif

static int test_phpbug51647(MYSQL *my)
{
  MYSQL* mysql;

  if (check_skip_ssl())
    return SKIP;

  mysql= mysql_init(NULL);
  FAIL_IF(!mysql, "Can't allocate memory");

  mysql_ssl_set(mysql, "C:/work/mariadb-native-client/unittest/libmariadb/certs/client-key.pem", 
                       "C:/work/mariadb-native-client/unittest/libmariadb/certs/client-cert.pem",
                       "C:/work/mariadb-native-client/unittest/libmariadb/certs/ca.pem", 0, 0);

  FAIL_IF(!mysql_real_connect(mysql, hostname, username, password, schema,
           port, socketname, 0), mysql_error(mysql));
  diag("%s", mysql_get_ssl_cipher(mysql));
  mysql_close(mysql);

  return OK;
}

static int test_conc50(MYSQL *my)
{
  MYSQL *mysql;

  if (check_skip_ssl())
    return SKIP;

  mysql= mysql_init(NULL);
  FAIL_IF(!mysql, "Can't allocate memory");

  mysql_ssl_set(mysql, NULL, NULL, "C:/work/mariadb-native-client/unittest/libmariadb/certs/my_cert.pem", NULL, NULL);

  mysql_real_connect(mysql, hostname, username, password, schema,
           port, socketname, 0);
  diag("Error: %d %s", mysql_errno(mysql), mysql_error(mysql));
  FAIL_IF(mysql_errno(mysql) != 2026, "Expected errno 2026");
  mysql_close(mysql);

  return OK;
}

static int test_conc50_1(MYSQL *my)
{
  MYSQL *mysql;

  if (check_skip_ssl())
    return SKIP;

  mysql= mysql_init(NULL);
  FAIL_IF(!mysql, "Can't allocate memory");

  mysql_ssl_set(mysql, NULL, NULL, "C:/work/mariadb-native-client/unittest/libmariadb/certs/ca.pem", NULL, NULL);

  mysql_real_connect(mysql, hostname, username, password, schema,
           port, socketname, 0);
  if (mysql_errno(mysql))
    diag("Error: %d %s", mysql_errno(mysql), mysql_error(mysql));
  FAIL_IF(mysql_errno(mysql), "No error expected");
  mysql_close(mysql);

  return OK;
}

static int test_conc50_2(MYSQL *my)
{
  MYSQL *mysql;

  if (check_skip_ssl())
    return SKIP;

  mysql= mysql_init(NULL);
  FAIL_IF(!mysql, "Can't allocate memory");

  mysql_ssl_set(mysql, NULL, NULL, "C:/work/mariadb-native-client/unittest/libmariadb/certs/dummy.pem", NULL, NULL);

  mysql_real_connect(mysql, hostname, username, password, schema,
           port, socketname, 0);
  FAIL_IF(mysql_errno(mysql) != 2026, "Expected errno 2026");
  mysql_close(mysql);

  return OK;
}

static int test_conc50_3(MYSQL *my)
{
  MYSQL *mysql;
  int rc;
  char query[256];

  if (check_skip_ssl())
    return SKIP;

  mysql_query(my, "DROP USER 'ssltest'@'localhost'");

  sprintf(query, "GRANT ALL ON %s.* TO 'ssltest'@'localhost' REQUIRE SSL", schema ? schema : "*");
  rc= mysql_query(my, query);
  check_mysql_rc(rc, my);
  rc= mysql_query(my, "FLUSH PRIVILEGES");
  check_mysql_rc(rc, my);

  mysql= mysql_init(NULL);
  FAIL_IF(!mysql, "Can't allocate memory");

  mysql_ssl_set(mysql, NULL, NULL, NULL, NULL, NULL);

  mysql_real_connect(mysql, hostname, (const char *)"ssltest", NULL, schema,
           port, socketname, 0);
  FAIL_IF(!mysql_errno(mysql), "Error expected, SSL connection required!");
  mysql_close(mysql);

  mysql= mysql_init(NULL);
  FAIL_IF(!mysql, "Can't allocate memory");

  mysql_ssl_set(mysql, NULL, NULL, "C:/work/mariadb-native-client/unittest/libmariadb/certs/ca.pem", NULL, NULL);

  mysql_real_connect(mysql, hostname, "ssltest", NULL, schema,
           port, socketname, 0);
  FAIL_IF(mysql_errno(mysql), "No error expected");
  mysql_close(mysql);

  return OK;
}

static int test_conc50_4(MYSQL *my)
{
  MYSQL *mysql;

  if (check_skip_ssl())
    return SKIP;

  mysql= mysql_init(NULL);
  FAIL_IF(!mysql, "Can't allocate memory");

  mysql_ssl_set(mysql, NULL, "C:/work/mariadb-native-client/unittest/libmariadb/certs/ca.pem", NULL, NULL, NULL);

  mysql_real_connect(mysql, hostname, username, password, schema,
           port, socketname, 0);
  FAIL_IF(mysql_errno(mysql) , "Expected no error");
  mysql_close(mysql);

  return OK;
}

static int verify_ssl_server_cert(MYSQL *my)
{
  MYSQL *mysql;
  uint verify= 1;

  if (check_skip_ssl())
    return SKIP;

  diag("certs needs to be fixed.");
  return SKIP;

  mysql= mysql_init(NULL);
  FAIL_IF(!mysql, "Can't allocate memory");

  mysql_ssl_set(mysql, NULL, NULL, "C:/work/mariadb-native-client/unittest/libmariadb/certs/ca.pem", NULL, NULL);
  mysql_options(mysql, MYSQL_OPT_SSL_VERIFY_SERVER_CERT, &verify);

  mysql_real_connect(mysql, hostname, username, password, schema,
           port, socketname, 0);

  FAIL_IF(!mysql_errno(mysql), "Expected error");
  mysql_close(mysql);

  return OK;
}

static int test_bug62743(MYSQL *my)
{
  MYSQL *mysql;

  if (check_skip_ssl())
    return SKIP;

  mysql= mysql_init(NULL);
  FAIL_IF(!mysql, "Can't allocate memory");

  mysql_ssl_set(mysql, "dummykey", NULL, NULL, NULL, NULL);

  mysql_real_connect(mysql, hostname, username, password, schema,
           port, socketname, 0);
  diag("Error: %s", mysql_error(mysql));
  FAIL_IF(mysql_errno(mysql) != 2026, "Expected errno 2026");
  mysql_close(mysql);

  mysql= mysql_init(NULL);
  FAIL_IF(!mysql, "Can't allocate memory");

  mysql_ssl_set(mysql, "C:/work/mariadb-native-client/unittest/libmariadb/certs/client-key.pem", NULL, NULL, NULL, NULL);

  mysql_real_connect(mysql, hostname, username, password, schema,
           port, socketname, 0);
  diag("Error with key: %s", mysql_error(mysql));
  FAIL_IF(mysql_errno(mysql) != 2026, "Expected errno 2026");
  mysql_close(mysql);

  mysql= mysql_init(NULL);
  FAIL_IF(!mysql, "Can't allocate memory");

  mysql_ssl_set(mysql, "C:/work/mariadb-native-client/unittest/libmariadb/certs/client-key.pem",
                       "C:/work/mariadb-native-client/unittest/libmariadb/certs/client-cert.pem", NULL, NULL, NULL);

  mysql_real_connect(mysql, hostname, username, password, schema,
           port, socketname, 0);
  FAIL_IF(mysql_errno(mysql) != 0, "Expected no error");
  mysql_close(mysql);

  mysql= mysql_init(NULL);
  FAIL_IF(!mysql, "Can't allocate memory");

  mysql_ssl_set(mysql, "C:/work/mariadb-native-client/unittest/libmariadb/certs/client-key.pem", "blablubb", NULL, NULL, NULL);

  mysql_real_connect(mysql, hostname, username, password, schema,
           port, socketname, 0);
  diag("Error with cert: %s", mysql_error(mysql));
  FAIL_IF(mysql_errno(mysql) == 0, "Expected error");
  mysql_close(mysql);

  return OK;
}

struct my_tests_st my_tests[] = {
  {"test_ssl", test_ssl, TEST_CONNECTION_NEW, 0,  NULL,  NULL},
  {"test_conc50", test_conc50, TEST_CONNECTION_NEW, 0,  NULL,  NULL},
  {"test_conc50_1", test_conc50_1, TEST_CONNECTION_NEW, 0,  NULL,  NULL},
  {"test_conc50_2", test_conc50_2, TEST_CONNECTION_NEW, 0,  NULL,  NULL},
  {"test_conc50_3", test_conc50_3, TEST_CONNECTION_NEW, 0,  NULL,  NULL},
  {"test_conc50_4", test_conc50_4, TEST_CONNECTION_NEW, 0,  NULL,  NULL},
  {"verify_ssl_server_cert", verify_ssl_server_cert, TEST_CONNECTION_NEW, 0,  NULL,  NULL},
  {"test_bug62743", test_bug62743, TEST_CONNECTION_NEW, 0,  NULL,  NULL}, 
  {"test_phpbug51647", test_phpbug51647, TEST_CONNECTION_NONE, 0, NULL, NULL},
  {"test_ssl_cipher", test_ssl_cipher, TEST_CONNECTION_NONE, 0,  NULL,  NULL},
  {"test_multi_ssl_connections", test_multi_ssl_connections, TEST_CONNECTION_NONE, 0,  NULL,  NULL},
#ifndef WIN32
  {"test_ssl_threads", test_ssl_threads, TEST_CONNECTION_NEW, 0,  NULL,  NULL},
#endif 

  {NULL, NULL, 0, 0, NULL, NULL}
};


int main(int argc, char **argv)
{
  get_envvars();

  if (argc > 1)
    get_options(argc, argv);


  run_tests(my_tests);

  mysql_server_end();
  return(exit_status());
}
