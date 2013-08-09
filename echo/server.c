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

#define MAX_CLIENTS 64

void chat_broadcast(const char *, size_t);
void err_msg(const char *);
void chat_welcome(connection_t *);

int event_accept(int);
void dump_conn(connection_t *);
connection_t * event_get_conn(int);
void event_read_handle(connection_t *);
void event_del_conn(connection_t *);
void event_init(int);
void event_process();

void set_nonblock(int fd) {
    int val;

    val = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, val | O_NONBLOCK);
}

struct pollfd *eventfd;
int maxi;
void * fd2conn[OPEN_MAX];

int main() {
    /* init socket */
    int i, listenfd;
    struct sockaddr_in servaddr;
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

    set_nonblock(listenfd);

    /*init event*/
    event_init(OPEN_MAX);

    /*add listenfd to event*/
    eventfd[0].fd = listenfd;
    eventfd[0].events = POLLRDNORM;

    maxi = 0;

    err_msg("start ...\n");
    event_process();

    return 0;
}

void event_process() {
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
void event_init(int max) {
    int i;

    eventfd = (struct pollfd  *) malloc(sizeof(struct pollfd) * max);

    for(i = 0; i < max; i++) {
	eventfd[i].fd = -1;
    }
}

void event_read_handle(connection_t *c) {
    ssize_t n;
    char buf[MAXLINE];

    if((n = read(c->fd, c->rbuf.i, &c->rbuf.buf[MAXLINE] - c->rbuf.i)) < 0) {
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
	if((n = c->rbuf.i - c->rbuf.o) > 0) {
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

void chat_broadcast(const char *buf, size_t n) {
    int j = 1;

    for(j = 1; j <= maxi; j++) {
	if(eventfd[j].fd < 0) {
	    continue;
	}

	conn_write(fd2conn[eventfd[j].fd], buf, n);
    }
}

void chat_welcome(connection_t *c){
    static char s[] = "WELCOME!";

    conn_write(c, s, strlen(s));
}
