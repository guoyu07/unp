#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <poll.h>
#include <strings.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>
#include "echo.h"

#define MAX_CLIENTS 64

struct connection_s {
    int fd;
    struct sockaddr_in ip;
    time_t time;
};
typedef struct connection_s connection_t;

void chat_send(connection_t *, const char *, size_t);
void chat_broadcast(const char *, size_t);
void err_msg(const char *);

connection_t *client_ptr;

int main() {
    /* init socket */
    int sockfd, maxi, i, connfd, listenfd, nready;
    ssize_t n;
    struct sockaddr_in servaddr, cliaddr;
    socklen_t clilen;
    struct pollfd client[OPEN_MAX];
    char buf[MAXLINE];

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SERV_PORT); 

    if((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	perror("socket");
	exit(1);
    }
    
    if(bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
	perror("bind");
	exit(1);
    }

    if(listen(listenfd, MAX_CLIENTS) < 0) {
	perror("listen");
	exit(1);
    }

    client[0].fd = listenfd;
    client[0].events = POLLRDNORM;
    for(i = 1; i < OPEN_MAX; i++) {
	client[i].fd = -1;
    }
    maxi = 0;

    err_msg("start ...\n");
    for( ; ; ) {
	nready = poll(client, maxi + 1, INFTIM);
	
	if(client[0].revents & POLLRDNORM) {
	    clilen = sizeof(cliaddr);
	    connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen);

	    for(i = 1; i < OPEN_MAX; i++) {
		if(client[i].fd < 0) {
		    client[i].fd = connfd;
		    printf("connection from %s:%d at %d\n",
			inet_ntop(AF_INET, &cliaddr.sin_addr, buf, sizeof(buf)),
			ntohs(cliaddr.sin_port), connfd);
		    break;
		}
	    }

	    if(i == OPEN_MAX) {
		err_msg("too many clients");
		close(connfd);
		break;
	    }

	    client[i].events = POLLRDNORM;
	    if(i > maxi)
		maxi = i;

	    if(--nready <= 0)
		continue;

	}

	for(i = 1; i <= maxi; i++) {
	    if((sockfd = client[i].fd) < 0)
		continue;

	    if(client[i].revents & (POLLRDNORM | POLLERR)) {
		n = read(sockfd, buf, MAXLINE);
		if(n < 0) {
		    if(errno == ECONNRESET) {
			close(sockfd);
			client[i].fd = -1;
		    } else {
			err_msg("read error");
			continue;
		    }
		} else if(n == 0) {
		    /*client send FIN*/
		    printf("client down at %d\n", client[i].fd);
		    close(sockfd);
		    client[i].fd = -1;
		} else
		    write(sockfd, buf, n);

		if(--nready <= 0) {
		    break;
		}
	    }
	}
    }
/* init connection pool */
    client_ptr = (connection_t *) malloc(sizeof(connection_t) * MAX_CLIENTS);
    for(i = 0; i < MAX_CLIENTS; i++) {
	(client_ptr + i)->fd = -1;
    }
    free(client_ptr);
}

void err_msg(const char *buf) {
    //char buf[MAXLINE + 1];

    fputs(buf, stderr);
    fflush(stderr);
    
}

void chat_send(connection_t *c, const char *buf, size_t n) {
    if (c->fd == -1) 
	return;

    write(c->fd, buf, n);
}

void chat_broadcast(const char *buf, size_t n) {
    int i;
    for(i = 0; i < MAX_CLIENTS; i++) {
	chat_send(client_ptr + i, buf, n);
    } 
}
