#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/types.h>

#include "proxy_cache/proxy_cache.h"
#include "proxy_serve/proxy_serve.h"
#include "socket_helpers/socket_helpers.h"

void safeFree(void **pp);
#define safe_free(ptr) safeFree((void **)&(ptr))

static void *serve_client(void *);
/*
 *   handle every client request by parsing them, check if valid and handle
 *   invalid requests, send a request to the sever then server back response to the client.
 */

static void free_resources(requestInfo *clientRequest, responseInfo *serverResponse, int clientfd);

cache proxyCache;

int main(int argc, char **argv)
{
    pthread_t tid;
    int listenfd, clientlen;
    int *clientfd;
    struct sockaddr_storage clientaddr;
    char hostname[MAXLINE], port[MAXLINE];

    if (argc != 2)
    {
        printf("Invalid argument\n");
        return 0;
    }

    signal(SIGPIPE, SIG_IGN);
    cache_init(&proxyCache);
    listenfd = open_listenfd(argv[1]);

    if (listenfd < 0)
        return -1;

    while (1)
    {
        clientlen = sizeof(clientaddr);

        clientfd = malloc(sizeof(int));
        *clientfd = accept(listenfd, (SA *)&clientaddr, (socklen_t *)&clientlen);

        if (*clientfd < 0)
            return -1;

        getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);

        printf("Accepted connection from (%s, %s)\n", hostname, port);

        pthread_create(&tid, NULL, serve_client, ((void *)clientfd));
    }

    close(listenfd);
    return 0;
}

static void *serve_client(void *vargp)
{
    int clientfd = *(int *)vargp;
    safe_free(vargp);
    requestInfo clientRequest;
    responseInfo serverResponse;

    pthread_detach(pthread_self());

    memset(&clientRequest, 0, sizeof(clientRequest));
    memset(&serverResponse, 0, sizeof(serverResponse));

    ((parse_request(clientfd, &clientRequest) < 0) || (serve_request(clientfd, &clientRequest, &serverResponse, &proxyCache) < 0) || (client_respond(clientfd, &serverResponse) < 0));
    free_resources(&clientRequest, &serverResponse, clientfd);
    
    return NULL;
}

static void free_resources(requestInfo *clientRequest, responseInfo *serverResponse, int clientfd)
{
    safe_free(clientRequest->host);
    safe_free(clientRequest->path);
    safe_free(clientRequest->port);
    safe_free(clientRequest->headers);
    safe_free(serverResponse->body);
    safe_free(serverResponse->headers);
    close(clientfd);
}

void safeFree(void **pp)
{
    if (pp != NULL && *pp != NULL)
    {
        free(*pp);
        *pp = NULL;
    }
}