#include <string.h>
#include <stdio.h>
#include "connection.h"
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>

static connection_t *connections;
static connection_t *free_conn;
static int free_conn_n;

static ssize_t conn_flush_wbuf(connection_t *c);

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

/* return wbuf len, if > 0, then need add event write*/
ssize_t conn_write(connection_t *c, const char *buf, size_t n) {
    int i;
    ssize_t nwritten;

    /*no data in write buf*/
    if(c->wbuf.i == c->wbuf.o) {
	if((nwritten = write(c->fd, buf, n)) < 0) {
	    /* socket send buffer has no space*/
	    if(errno == EWOULDBLOCK) {
		i = &c->wbuf.buf[MAXLINE] - c->wbuf.i;
		/* data was large then wbuf, drop it*/
		if(i < n) {
		    return 0;
		}
		memcpy(c->wbuf.i, buf, n);
		c->wbuf.i += n;
		return n;
	    } else {
		fprintf(stderr, "write error to socket");
		return -1;
	    }
	}

	if (nwritten == n) return 0;

	i = &c->wbuf.buf[MAXLINE] - c->wbuf.i;
	/*
	 * The remaining data is too large to buffer can't hold
	 * This situation should be avoided
	 */
	if(i < (n - nwritten)) {
	    return 0;
	}

	i = n - nwritten;
	memcpy(c->wbuf.i, buf + nwritten, i);
	c->wbuf.i += i;
	return i;
    }

    i = &c->wbuf.buf[MAXLINE] - c->wbuf.i;
    /*
     * wbuf's free space is not enough to storage, drop it or other
     */
    if(i < n) {
	/*
	 * @todo try write wbuf data to socket, may be realse some space
	 */
	conn_flush_wbuf(c);
	return -1;
    }
    memcpy(c->wbuf.i, buf, n);
    c->wbuf.i += n;

    i = c->wbuf.i - c->wbuf.o;
    if((nwritten = write(c->fd, c->wbuf.o, i)) < 0) {
	/* socket send buffer has no space*/
	if(errno == EWOULDBLOCK) {
	    return i;
	} else {
	    fprintf(stderr, "write error to socket");
	    return -1;
	}
    }

    c->wbuf.o += nwritten;
    if(c->wbuf.o == c->wbuf.i) {
	c->wbuf.o = c->wbuf.i = c->wbuf.buf;
	return 0;
    }

    return c->wbuf.i - c->wbuf.o;
}

ssize_t conn_flush_wbuf(connection_t *c) {
    return c->wbuf.i - c->wbuf.o;
}
