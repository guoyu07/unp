#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <math.h>
#include "echo.h"

void str_cli(FILE *, int);

int main(int argc, char *argv[]) {
    int sockfd;
    struct sockaddr_in servaddr;

    if(argc != 2) {
	printf("Usage cmd ip");
	exit(1);
    }

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    inet_pton(AF_INET, argv[1], &servaddr.sin_addr);
    servaddr.sin_port = htons(SERV_PORT);
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));

    str_cli(stdin, sockfd);
    exit(0);
}

void str_cli(FILE *fp, int fd) {
    int maxfdp1;
    fd_set rset;
    char sendline[MAXLINE], recvline[MAXLINE];

    FD_ZERO(&rset);
    for( ; ; ) {
	FD_SET(fileno(fp), &rset);
	FD_SET(fd, &rset);
	maxfdp1 = max(fileno(fp), fd) + 1;
	select(maxfdp1, &rset, NULL, NULL, NULL);

	if(FD_ISSET(fd, &rset)) {
	    if(read(fd, recvline, MAXLINE) == 0) {
		printf("str_cli: server terminated prematurely\n");
		exit(1);
	    }
	    fputs(recvline, stdout);
	}

	if(FD_ISSET(fileno(fp), &rset)) {
	    if(fgets(sendline, MAXLINE, fp) == NULL) {
		printf("fgets return NULL\n");
		return;
	    }
	    printf("write something\n");
	    write(fd, sendline, strlen(sendline));
	    printf("w\n");
	}
    }
}
