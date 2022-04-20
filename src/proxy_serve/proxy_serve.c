#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "proxy_serve.h"
#include "../socket_helpers/socket_helpers.h"
#include "../robust_input_output/rio.h"

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:98.0) Gecko/20100101 Firefox/98.0\r\n";
static const char *connection_hdr = "Connection: close\r\n";
static const char *proxy_connection_hdr = "Proxy-Connection: close\r\n";

static int parse_request_line(int clientfd, rio_t *rio, char *uri);
/*
 *  Parse request line, extract the uri and check if
 *  the request contains valid method and valid http version.
 *  Return 0 if valid and -1 other ways.
 */

static int parse_headers(rio_t *rio, char **requestHeaders);
/*
 *  Parse client request, extract the request headers and add missing headers.
 */

static void parse_url(const char *url, char **hostname, char **port, char **path);
/*
 *  Parse uri to extract the request information.
 *  Return 0 if valid and -1 other ways.
 */

static int parse_response(rio_t *rio, unsigned int *contentLength, char **responseHeaders, void **responseContent);
/*
 *  Parse server response to extract the response headers and body.
 *  Return 0 if valid and -1 other ways.
 */

static void build_request_line(char *path, char *requestLine);
/*
 *  Compine the method, path and http version to build request line.
 */

static int resend_request(int serverfd, char *requestLine, char *requestHeaders);
/*
 *   In case of a miss it forward the request to the server.
 *  Return 0 if valid and -1 other ways.
 */

static void client_error(int clientfd, char *cause, char *errnum, char *short_msg, char *long_msg);
/*
 *   Build an html page to print the error message.
 */

int parse_request(int clientfd, requestInfo *clientRequest)
{
    rio_t rio;
    char uri[MAXLINE];

    rio_readinitb(&rio, clientfd);

    if (parse_request_line(clientfd, &rio, uri) < 0)
        return -1;

    parse_url(uri, &clientRequest->host, &clientRequest->port, &clientRequest->path);

    if (parse_headers(&rio, &clientRequest->headers) < 0)
        return -1;

    return 0;
}

int serve_request(int clientfd, requestInfo *clientRequest, responseInfo *serverResponse, cache *proxyCache)
{
    rio_t rio;
    int serverfd;
    unsigned long tag;
    char requestLine[MAXLINE];

    build_request_line(clientRequest->path, requestLine);

    if (!(cache_fetch(proxyCache, &tag, requestLine, clientRequest->headers, &serverResponse->contentLength,
                      &serverResponse->headers, &serverResponse->body)))
    {
        printf("cache hit!!\n");
        return 0;
    }

    printf("cache miss!!\n");

    if ((serverfd = open_clientfd(clientRequest->host, clientRequest->port)) < 0)
    {
        client_error(clientfd, "GET", "400", "Bad request", "Fail to connect");
        return -1;
    }

    rio_readinitb(&rio, serverfd);

    if ((resend_request(serverfd, requestLine, clientRequest->headers) < 0) ||
        (parse_response(&rio, &serverResponse->contentLength, &serverResponse->headers, &serverResponse->body) < 0))
    {
        close(serverfd);
        return -1;
    }

    close(serverfd);
    cache_write(proxyCache, tag, serverResponse->contentLength, serverResponse->headers, serverResponse->body);
    return 0;
}

int client_respond(int clientfd, responseInfo *serverResponse)
{
    if ((rio_writen(clientfd, serverResponse->headers, strlen(serverResponse->headers)) < 0) ||
        (rio_writen(clientfd, serverResponse->body, serverResponse->contentLength) < 0))
        return -1;

    return 0;
}

static int parse_request_line(int clientfd, rio_t *rio, char *uri)
{
    char buf[MAXBUF], method[MAXLINE], version[MAXLINE];

    rio_readlineb(rio, buf, MAXBUF);

    sscanf(buf, "%s %s %s", method, uri, version);

    if (strcasecmp(method, "GET"))
    {
        client_error(clientfd, method, "501", "Not Implemented", "The proxy does not implement this method");
        return -1;
    }
    if (strcasecmp(version, "HTTP/1.0") && strcasecmp(version, "HTTP/1.1"))
    {
        client_error(clientfd, version, "400", "Bad request", "Invalid HTTP version");
        return -1;
    }
    return 0;
}

static void
parse_url(const char *url, char **hostname, char **port, char **path)
{
    char *url_copy, *url_token;

    url_copy = strdup(url);
    url_token = url_copy;

    /* Skip the "http://" part of the url */
    url_token = strstr(url, "://");
    url_token += 3;

    if (strstr(url_token, ":"))
    { /* Port is contained in the url */
        /* Url token points to the hostname */
        url_token = strtok(url_token, ":");
        *hostname = strdup(url_token);

        /* Move url token to the port and path token */
        url_token = strtok(NULL, ":");

        if (strstr(url_token, "/"))
        { /* Path is contained in the url */
            /* Parse the port from url_token */
            url_token = strtok(url_token, "/");
            *port = strdup(url_token);

            /* Parse the path from url_token */
            url_token = strtok(NULL, "/");
            *path = strdup(url_token);
        }
        else
        { /* Path is not contained in the url */
            *port = strdup(url_token);
            *path = strdup("/");
        }
    }
    else
    { /* Port is not contained in the url */
        *port = strdup("80");
        url_token = strtok(url_token, "/");
        *hostname = strdup(url_token);

        url_token = strtok(NULL, "/");

        if (url_token) /* Path is contained in the url */
            *path = strdup(url_token);
        else /* Path is not contained in the url */
            *path = strdup("/");
    }
    free(url_copy);
}

static int parse_headers(rio_t *rio, char **requestHeaders)
{
    char buf[MAXBUF], headers[MAXBUF];

    headers[0] = '\0';
    strcat(headers, user_agent_hdr);
    strcat(headers, connection_hdr);
    strcat(headers, proxy_connection_hdr);

    do
    {
        if (rio_readlineb(rio, buf, MAXBUF) < 0)
            return -1;
        strcat(headers, buf);

    } while (strcmp(buf, "\r\n"));

    *requestHeaders = strdup(headers);
    return 0;
}

static void build_request_line(char *path, char *requestLine)
{
    sprintf(requestLine, "GET %s HTTP/1.0\r\n", path);
}

static int parse_response(rio_t *rio, unsigned int *contentLength, char **responseHeaders, void **responseContent)
{
    char buf[MAXBUF], headers_buf[MAXBUF];
    headers_buf[0] = '\0';

    do
    {
        if (rio_readlineb(rio, buf, MAXBUF) < 0)
            return -1;

        ((sscanf(buf, "Content-length: %u", contentLength) == 1) ||
         (sscanf(buf, "Content-Length: %u", contentLength) == 1) ||
         (sscanf(buf, "content-length: %u", contentLength) == 1));

        strcat(headers_buf, buf);

    } while (strcmp(buf, "\r\n"));

    *responseHeaders = strdup(headers_buf);
    *responseContent = malloc((long unsigned int)*contentLength);
    if (rio_readnb(rio, *responseContent, *contentLength) < 0)
        return -1;

    return 0;
}

static int resend_request(int serverfd, char *requestLine, char *requestHeaders)
{
    if ((rio_writen(serverfd, requestLine, strlen(requestLine)) < 0) ||
        (rio_writen(serverfd, requestHeaders, strlen(requestHeaders)) < 0))
        return -1;
    return 0;
}

static void
client_error(int clientfd, char *cause, char *errnum,
             char *short_msg, char *long_msg)
{
    char linebuf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Proxy Error</title>");
    strcat(body, "<body bgcolor="
                 "ffffff"
                 ">\r\n");
    sprintf(linebuf, "%s: %s\r\n", errnum, short_msg);
    strcat(body, linebuf);
    sprintf(linebuf, "%s: %s\r\n", long_msg, cause);
    strcat(body, linebuf);

    /* Send the HTTP response */
    sprintf(linebuf, "HTTP/1.0 %s %s\r\n", errnum, short_msg);
    if (rio_writen(clientfd, linebuf, strlen(linebuf)) < 0)
        return;
    sprintf(linebuf, "Content-Type: text/html\r\n");
    if (rio_writen(clientfd, linebuf, strlen(linebuf)) < 0)
        return;
    sprintf(linebuf, "Content-Length: %zu\r\n\r\n", strlen(body));
    if (rio_writen(clientfd, linebuf, strlen(linebuf)) < 0)
        return;
    if (rio_writen(clientfd, body, strlen(body)) < 0)
        return;
}