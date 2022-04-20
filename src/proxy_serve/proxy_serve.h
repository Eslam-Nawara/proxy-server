#ifndef __PROXY_SERVER_H__
#define __PROXY_SERVER_H__

#include <stdio.h>
#include "../proxy_cache/proxy_cache.h"

typedef struct
{
    char *host;
    char *path;
    char *port;
    char *headers;
} requestInfo;

typedef struct
{
    void *body;
    char *headers;
    unsigned int contentLength;

} responseInfo;

#define MAXLINE 1024 /* Max text line length */
#define MAXBUF 8192  /* Max text line length */

int parse_request(int clientfd, requestInfo *clientRequest);
/*
 *  Parse the client requests to get the host name, the file path, the port number
 *  and the other headers.
 *  Return 0 if completed with no error, -1 other ways.
 */

int serve_request(int clientfd, requestInfo *clientRequest, responseInfo *serverResponse, cache *proxyCache);
/*
 *  Build headers for the request then connect to the server, send request and send
 *  the headers and return the descriptor.
 *  Return 0 if completed with no error, -1 other ways.
 */

int client_respond(int clientfd, responseInfo *serverResponse);
/*
 *  Send back the server response to the client.
 *  2close both descriptors.
 *  Return 0 if completed with no error, -1 other ways.
 */

#endif