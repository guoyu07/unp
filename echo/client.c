#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <math.h>
#include <fcntl.h>
#include <errno.h>
#include "echo.h"

void str_cli(FILE *, int);

void set_nonblock(int fd) {
    int val;

    val = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, val | O_NONBLOCK);
}

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
    if(connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
	perror("connect");
	exit(1);
    }

    str_cli(stdin, sockfd);
    exit(0);
}

void str_cli(FILE *fp, int sockfd) {
    int maxfdp1;
    ssize_t n, nwritten;
    fd_set rset, wset;
    char to[MAXLINE], fr[MAXLINE];
    char *toiptr, *tooptr, *friptr, *froptr;

    toiptr = tooptr = to;
    friptr = froptr = fr;

    set_nonblock(sockfd);
    set_nonblock(STDIN_FILENO);

    maxfdp1 = max(STDIN_FILENO, sockfd) + 1;

    for( ; ; ) {
	FD_ZERO(&rset);
	FD_ZERO(&wset);

	/*if to buf had some space yet, then*/
	if(toiptr < &to[MAXLINE]) {
	    FD_SET(STDIN_FILENO, &rset);
	}
	if(friptr < &fr[MAXLINE]) {
	    FD_SET(sockfd, &rset);
	}

	/*如果缓冲区中有数据未发送完，需要注册写事件*/
	if(tooptr != toiptr) {
	    FD_SET(sockfd, &wset);
	}
	if(froptr != friptr) {
	    FD_SET(STDOUT_FILENO, &wset);
	}

	select(maxfdp1, &rset, &wset, NULL, NULL);

	if(FD_ISSET(sockfd, &rset)) {
	    if((n = read(sockfd, friptr, &fr[MAXLINE] - friptr)) < 0) {
		if(errno != EWOULDBLOCK) {
		    fprintf(stderr, "str_cli: read sock error\n");
		    exit(1);
		}
	    } else if(n == 0) {
		/*EOF on socket*/
		fprintf(stderr, "str_cli: server terminated prematurely\n");
		exit(1);
	    } else {
		fprintf(stderr, "read %d byte from socket\n", n);
		friptr += n;

		/*listen stdout write*/
		FD_SET(STDOUT_FILENO, &wset);
	    }
	}

	if(FD_ISSET(STDIN_FILENO, &rset)) {
	    if((n = read(STDIN_FILENO, toiptr, &to[MAXLINE] - toiptr)) < 0) {
		if(errno != EWOULDBLOCK) {
		    fprintf(stderr, "read stdin error\n");
		    exit(1);
		}
	    } else if(n == 0) {
		/* EOF on stdin
		 * if to buf is empty,then close socket write endian
		 * send FIN
		 */
		if(tooptr == toiptr) {
		    shutdown(sockfd, SHUT_WR);
		}
	    } else {
		toiptr += n;
		FD_SET(sockfd, &wset);
	    }
	}

	if(FD_ISSET(STDOUT_FILENO, &wset) && ((n = friptr - froptr) > 0)) {
	    if((nwritten = write(STDOUT_FILENO, froptr, n)) < 0) {
		if(errno != EWOULDBLOCK) {
		    fprintf(stderr, "write stdout error\n");
		    exit(1);
		}
	    } else {
		froptr += nwritten;
		if(froptr == friptr) froptr = friptr = fr;
	    }
	}

	if(FD_ISSET(sockfd, &wset) && ((n = toiptr - tooptr) > 0)) {
	    if((nwritten = write(sockfd, tooptr, n)) < 0) {
		if(errno != EWOULDBLOCK) {
		    fprintf(stderr, "write sockfd error\n");
		    exit(1);
		}
	    } else {
		tooptr += nwritten;
		if(tooptr == toiptr) tooptr = toiptr = to;
	    }
	
	}

    }
}
