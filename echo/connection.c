#include <string.h>
#include <stdio.h>
#include "connection.h"
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>

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

void conn_write(connection_t *c, const char *buf, size_t n) {
    int i;
    size_t nwritten;

    /*no data in write buf*/
    if(c->wbuf.i == c->wbuf.o) {
	if((nwritten = write(c->fd, buf, n)) < 0) {
	    /* socket send buffer has no space*/
	    if(errno == EWOULDBLOCK) {
		i = &c->wbuf.buf[MAXLINE] - c->wbuf.i;
		/* data was large then wbuf, drop it*/
		if(i < n) {
		    return;
		}
		memcpy(c->wbuf.i, buf, n);
		c->wbuf.i += n;
		/* 
		 * @todo add write on eventfd
		 */
		return;
	    } else {
		fprintf(stderr, "write error to socket");
		return;
	    }
	}

	if (nwritten == n) return;

	i = &c->wbuf.buf[MAXLINE] - c->wbuf.i;
	if(i < (n - nwritten)) {
	    /**
	     * @todo
	     */
	    return;
	}
	i = n - nwritten;

	memcpy(c->wbuf.i, buf + nwritten, i);
	c->wbuf.i += i;
	return;
    }

    i = &c->wbuf.buf[MAXLINE] - c->wbuf.i;
    if(i < n) {
	return;
    }
    memcpy(c->wbuf.i, buf, n);
    c->wbuf.i += n;

    i = c->wbuf.i - c->wbuf.o;
    if((nwritten = write(c->fd, c->wbuf.o, i)) < 0) {
	/* socket send buffer has no space*/
	if(errno == EWOULDBLOCK) {
	    return;
	} else {
	    fprintf(stderr, "write error to socket");
	    return;
	}
    }

    c->wbuf.o += nwritten;
    if(c->wbuf.o == c->wbuf.i)
	c->wbuf.o = c->wbuf.i = c->wbuf.buf;
}

