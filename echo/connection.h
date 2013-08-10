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

#define conn_buf_data_len(buf) (buf.i - buf.o)
#define conn_buf_free_len(buf) (&buf.data[CONN_BUF_LENGTH] - buf.i)
#define conn_buf_rewind(buf) (buf.i = buf.o = buf.data)

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

#endif /*_CONNECTION_H*/
