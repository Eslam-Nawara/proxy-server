#ifndef _SOCKET_HELPERS_H_
#define _SOCKET_HELPERS_H_

typedef struct sockaddr SA;

/* Reentrant protocol-independent client/server helpers */
int open_clientfd(char *hostname, char *port);
int open_listenfd(char *port);

#endif