#ifndef _ECHO_H
#define _ECHO_H 1

#define SERV_PORT 7789
#define MAXLINE 1024

#ifndef OPEN_MAX
#   define OPEN_MAX 1024
#endif

#ifndef INFTIM
#   define INFTIM -1
#endif

#ifndef max
#   define max(a, b) (a > b) ? a : b
#endif

#endif /*_ECHO_H*/
