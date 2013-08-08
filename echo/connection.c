#include "connection.h"
#include <stdlib.h>
#include <stddef.h>

static connection_t *connections;
static connection_t *free_conn;
static int free_conn_n;

connection_t * conn_init_pool(int n) {
    connection_t *c, *next;
    int i;

    connections = (connection_t *) malloc(sizeof(connection_t) * n);
    if(connections == NULL) return NULL;

    i = n;
    c = connections;
    next = NULL;

    do {
	i--;

	c[i].data = next;
	next = &c[i];
	
	c[i].fd = -1;
	c[i].rbuf.o = c[i].rbuf.i = c[i].rbuf.buf;
	c[i].wbuf.o = c[i].wbuf.i = c[i].wbuf.buf;
    
    } while (i);
    
    free_conn = next;
    free_conn_n = n;
    
    return c;
}

connection_t * conn_get(int fd) {
    connection_t *c;

    c = free_conn;
    if(c == NULL) {
	return NULL;
    }

    free_conn = c->data;
    free_conn_n--;

    c->fd = fd;
    return c;
}

void conn_free(connection_t *c) {
    c->data = free_conn;
    free_conn = c;
    free_conn_n++;
}
