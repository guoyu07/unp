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
#include "echo.h"
#include "connection.h"

#define MAX_CLIENTS 64


void chat_send(connection_t *, const char *, size_t);
void chat_broadcast(const char *, size_t);
void err_msg(const char *);
void chat_welcome(int);

int event_accept(int);
void dump_conn(connection_t *);
connection_t * event_get_conn(int);
void event_read_handle(connection_t *);
void event_del_conn(connection_t *);

struct pollfd *eventfd;
int maxi;
void * fd2conn[OPEN_MAX];

int main() {
    /* init socket */
    int sockfd, i, connfd, listenfd, nready;
    ssize_t n;
    struct sockaddr_in servaddr, cliaddr;
    socklen_t clilen;
    
    char buf[MAXLINE];
    connection_t *c;

    /* init connection pool */
    c = conn_init_pool(MAX_CLIENTS);
    if(c == NULL) {
	fprintf(stderr, "connections pool allow mem error\n");
	return 1; 
    }
    
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SERV_PORT); 

    if((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	perror("socket");
	exit(1);
    }

    i = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i));
    if(bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
	perror("bind");
	exit(1);
    }

    if(listen(listenfd, MAX_CLIENTS) < 0) {
	perror("listen");
	exit(1);
    }

    /*init event*/
    eventfd = (struct pollfd  *) malloc(sizeof(struct pollfd) * OPEN_MAX);

    eventfd[0].fd = listenfd;
    eventfd[0].events = POLLRDNORM;

    for(i = 1; i < OPEN_MAX; i++) {
	eventfd[i].fd = -1;
    }
    maxi = 0;

    err_msg("start ...\n");
    for( ; ; ) {
	nready = poll(eventfd, maxi + 1, INFTIM);
	
	if(eventfd[0].revents & POLLRDNORM) {
	    event_accept(listenfd);
	    if(--nready <= 0)
		continue;
	}

	for(i = 1; i <= maxi; i++) {
	    if((sockfd = eventfd[i].fd) < 0)
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

void event_read_handle(connection_t *c) {
    ssize_t n;
    char buf[MAXLINE];

    if((n = read(c->fd, c->rbuf.i, &c->rbuf.buf[MAXLINE] - c->rbuf.i)) < 0) {
	if(errno == ECONNRESET) {
	    event_del_conn(c);
	} else {
	    err_msg("read error");
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
	if((n = c->rbuf.i - c->rbuf.o) > 10) {
	    chat_broadcast(c->rbuf.o, n);
	    c->rbuf.o = c->rbuf.i = c->rbuf.buf;
	}
    }
}

connection_t * event_get_conn(int fd) {
    return (connection_t *)fd2conn[fd];
}

int event_accept(int fd) {
    int connfd;
    socklen_t clilen;
    struct sockaddr cliaddr;
    connection_t *c;

    clilen = sizeof(cliaddr);
    connfd = accept(fd, (struct sockaddr *)&cliaddr, &clilen);

    c = conn_get(connfd);
    if (c == NULL) {
	fprintf(stderr, "conn not en!\n");
	close(connfd);
	return 1;
    }

    c->fd = connfd;
    c->time = time(NULL);
    memcpy(&c->ip, &cliaddr, clilen);
    c->len = clilen;

    dump_conn(c);

    /*say hello*/
    chat_welcome(connfd);
    return event_add_conn(c);
}

void event_del_conn(connection_t *c) {
    fd2conn[c->fd] = NULL;
    eventfd[c->index].fd = -1;
    close(c->fd);
    conn_free(c);
}

int event_add_conn(connection_t *c) {
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

void dump_conn(connection_t *c) {
    char buf[MAXLINE];
    struct sockaddr_in *s;
    
    s = (struct sockaddr_in *)&c->ip;

    printf("-%.24s connection from %s:%d at %d\n",
	ctime(&c->time),
	inet_ntop(AF_INET, &s->sin_addr, buf, sizeof(buf)),
	ntohs(s->sin_port), c->fd);
}

void err_msg(const char *buf) {
    //char buf[MAXLINE + 1];

    fputs(buf, stderr);
    fflush(stderr);
    
}

void chat_send(connection_t *c, const char *buf, size_t n) {
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

void chat_broadcast(const char *buf, size_t n) {
    int j = 1;

    for(j = 1; j <= maxi; j++) {
	if(eventfd[j].fd < 0) {
	    continue;
	}

	chat_send(fd2conn[eventfd[j].fd], buf, n);
    }
}

void chat_welcome(int fd){
    static char c[] = "WELCOME!\n";
    write(fd, c, strlen(c));
}
