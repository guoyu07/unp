#include <netinet/in.h>
#include "echo.h"

#ifndef _CONNECTION_H
#define _CONNECTION_H 1

typedef struct{
    char *o;
    char *i;
    char buf[MAXLINE];
} conn_buff_t;

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

#endif /*_CONNECTION_H*/
