#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>
#include "connection.h"

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
	CONN_BUF_REWIND(c[i].rbuf);
	CONN_BUF_REWIND(c[i].wbuf);
    
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
		i = CONN_BUF_FREE_LEN(c->wbuf);
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

	i = CONN_BUF_FREE_LEN(c->wbuf);
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

    i = CONN_BUF_FREE_LEN(c->wbuf);
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

    i = CONN_BUF_DATA_LEN(c->wbuf);
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
	CONN_BUF_REWIND(c->wbuf);
	return 0;
    }

    return CONN_BUF_DATA_LEN(c->wbuf);
}

ssize_t conn_flush_wbuf(connection_t *c) {
    return CONN_BUF_DATA_LEN(c->wbuf);
}

void
dump_conn(connection_t *c) 
{
    char buf[MAXLINE];
    struct sockaddr_in *s;
    
    s = (struct sockaddr_in *)&c->ip;

    printf("-%.24s connection from %s:%d at %d\n",
	ctime(&c->time),
	inet_ntop(AF_INET, &s->sin_addr, buf, sizeof(buf)),
	ntohs(s->sin_port), c->fd);
}
