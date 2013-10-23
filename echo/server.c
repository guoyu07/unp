#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <strings.h>
#include <stdlib.h>

#include "echo.h"
#include "connection.h"
#include "event.h"

#define MAX_CLIENTS 64

void err_msg(const char *);
static int tcp_listen(unsigned short);

int
main()
{
    /* init socket */
    int listenfd;
    connection_t *c;

    /* init connection pool */
    c = conn_init_pool(MAX_CLIENTS);
    if(c == NULL) {
	fprintf(stderr, "connections pool allow mem error\n");
	return 1; 
    }

    /*init event*/
    event_init(OPEN_MAX);

    listenfd = tcp_listen(SERV_PORT);
    
    /*add listenfd to event*/
    event_add_listenfd(listenfd);

    err_msg("start ...\n");
    event_process();

    return 0;
}

static int
tcp_listen(unsigned short port)
{
    struct sockaddr_in servaddr;
    int listenfd, i;

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port); 

    if((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	perror("socket");
	exit(1);
    }

    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i));
    if(bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
	perror("bind");
	exit(1);
    }

    if(listen(listenfd, MAX_CLIENTS) < 0) {
	perror("listen");
	exit(1);
    }

    return listenfd;
}

void 
err_msg(const char *buf) 
{
    //char buf[MAXLINE + 1];

    fputs(buf, stderr);
    fflush(stderr);
    
}
