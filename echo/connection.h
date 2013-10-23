#include <netinet/in.h>
#include "echo.h"

#ifndef _CONNECTION_H
#define _CONNECTION_H 1

#define CONN_BUF_LENGTH 1024

typedef struct{
    char *o;
    char *i;
    char data[CONN_BUF_LENGTH];
} conn_buff_t;

#define CONN_BUF_DATA_LEN(buf) (buf.i - buf.o)
#define CONN_BUF_FREE_LEN(buf) (&buf.data[CONN_BUF_LENGTH] - buf.i)
#define CONN_BUF_REWIND(buf) (buf.i = buf.o = buf.data)

struct connection_s {
    void *data;

    int fd;
    int index;
    conn_buff_t rbuf;
    conn_buff_t wbuf;

    struct sockaddr ip;
    socklen_t len;
    time_t time;
};
typedef struct connection_s connection_t;

connection_t * conn_init_pool(int);
connection_t * conn_get(int);
void conn_free(connection_t *);
ssize_t conn_write(connection_t *, const char *, size_t);
void dump_conn(connection_t *);

#endif /*_CONNECTION_H*/
