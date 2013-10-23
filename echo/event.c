#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <poll.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>
#include <fcntl.h>

#include "echo.h"
#include "connection.h"
#include "event.h"

static void chat_broadcast(const char *, size_t);
static void chat_welcome(connection_t *);

static int event_accept(int);
static connection_t * event_get_conn(int);
static void event_read_handle(connection_t *);
static void event_del_conn(connection_t *);
static void event_add_conn_write(connection_t *);
static void event_del_conn_write(connection_t *);
static int event_add_conn(connection_t *);

static void 
set_nonblock(int fd) 
{
    int val;

    val = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, val | O_NONBLOCK);
}

static struct pollfd *eventfd;
static int maxi;
static void * fd2conn[OPEN_MAX];

void 
event_process() 
{
    int nready, i;
    connection_t *c;

    for( ; ; ) {
	nready = poll(eventfd, maxi + 1, INFTIM);
	
	if(eventfd[0].revents & POLLRDNORM) {
	    event_accept(eventfd[0].fd);
	    if(--nready <= 0)
		continue;
	}

	for(i = 1; i <= maxi; i++) {
	    if(eventfd[i].fd < 0)
		continue;

	    if(eventfd[i].revents & (POLLRDNORM | POLLERR)) {
		c = event_get_conn(eventfd[i].fd);
		if(c->fd == -1) {
		    fprintf(stderr, "conn'fd is not validate -1");
		    continue;
		}

		event_read_handle(c);
		if(--nready <= 0) {
		    break;
		}
	    }
	}
    }
}

void 
event_init(int max) 
{
    int i;

    eventfd = (struct pollfd  *) malloc(sizeof(struct pollfd) * max);
    if(eventfd == NULL) {
	/*
	 * @todo malloc failed
	 */
    
    }

    for(i = 0; i < max; i++) {
	eventfd[i].fd = -1;
    }

    maxi = 0;
}

void
event_add_listenfd(int fd)
{
    set_nonblock(fd);

    eventfd[0].fd = fd;
    eventfd[0].events = POLLRDNORM;
}

static void
event_read_handle(connection_t *c) 
{
    ssize_t n;
    char buf[MAXLINE];

    if((n = read(c->fd, c->rbuf.i, CONN_BUF_FREE_LEN(c->rbuf))) < 0) {
	if(errno == ECONNRESET) {
	    event_del_conn(c);
	} else {
	    perror("read error");
	    return;
	}
    } else if(n == 0) {
	/*eventfd send FIN*/
	printf("client down at %d\n", c->fd);
	event_del_conn(c);
    } else {
	c->rbuf.i += n;
	printf("read %d byte from fd %d, buf len %d\n", n, c->fd, c->rbuf.i - c->rbuf.o);

	/*parse protocols*/
	if((n = CONN_BUF_DATA_LEN(c->rbuf)) > 0) {
	    chat_broadcast(c->rbuf.o, n);
	    CONN_BUF_REWIND(c->rbuf);
	}
    }
}

static connection_t * 
event_get_conn(int fd) 
{
    return (connection_t *)fd2conn[fd];
}

static int 
event_accept(int fd) 
{
    int connfd;
    socklen_t clilen;
    struct sockaddr cliaddr;
    connection_t *c;

    clilen = sizeof(cliaddr);
    /* fd is nonblock,
     * igrone the follow errno
     * EWOULDBLOCK,ECONNABORTED,EPROTO,EINTR
     */
    if((connfd = accept(fd, (struct sockaddr *)&cliaddr, &clilen)) < 0) {
	if(errno == EWOULDBLOCK || errno == ECONNABORTED || errno == EPROTO || errno == EINTR) {
	    return;
	}
	perror("accept error\n");
	return;
    }

    c = conn_get(connfd);
    if (c == NULL) {
	fprintf(stderr, "conn not en!\n");
	close(connfd);
	return 1;
    }
    
    set_nonblock(connfd);

    c->fd = connfd;
    c->time = time(NULL);
    memcpy(&c->ip, &cliaddr, clilen);
    c->len = clilen;

    dump_conn(c);

    /*say hello*/
    chat_welcome(c);
    return event_add_conn(c);
}

static void
event_del_conn(connection_t *c) 
{
    fd2conn[c->fd] = NULL;
    eventfd[c->index].fd = -1;
    close(c->fd);
    conn_free(c);
}

static int 
event_add_conn(connection_t *c) 
{
    int i;
 
    for(i = 1; i < OPEN_MAX; i++) {
	if(eventfd[i].fd < 0) {
	    eventfd[i].fd = c->fd;
	    break;
	}
    }

    if(i == OPEN_MAX) {
	fprintf(stderr, "too many clients");
	return -1;
    }

    eventfd[i].events = POLLRDNORM;
    fd2conn[c->fd] = c;
    c->index = i;

    if(i > maxi)
	maxi = i;

    return i;
}

static void 
chat_broadcast(const char *buf, size_t n) 
{
    int j = 1;
    ssize_t len;
    connection_t *c;

    for(j = 1; j <= maxi; j++) {
	if(eventfd[j].fd < 0) {
	    continue;
	}

	c = (connection_t *) fd2conn[eventfd[j].fd];

	len = conn_write(c, buf, n);
	if(len > 0) {
	    event_add_conn_write(c);
	} else if(len == 0) {
	    event_del_conn_write(c);
	}
    }
}

static void 
event_add_conn_write(connection_t *c) 
{
    eventfd[c->index].events |= POLLOUT;
}

static void 
event_del_conn_write(connection_t *c) 
{
    eventfd[c->index].events &= ~POLLOUT;
}

static void 
chat_welcome(connection_t *c)
{
    static char s[] = "WELCOME!";

    conn_write(c, s, strlen(s));
}
