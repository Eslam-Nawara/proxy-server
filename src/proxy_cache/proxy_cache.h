#ifndef __PROXY_CACHE_H__
#define __PROXY_CACHE_H__

#include <semaphore.h>
#include <pthread.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define CACHE_LINE_CNT (MAX_CACHE_SIZE / MAX_OBJECT_SIZE)

typedef struct
{
    unsigned char valid;
    unsigned long tag;
    unsigned int contentLength;
    unsigned long long timeStamp;
    char *responseHeaders;
    void *content;
} cacheLine;

typedef struct
{
    sem_t readCntMutex, writeMutex, teimeStampMutex;
    unsigned int readCnt;
    unsigned long long highestTimeStamp;
    cacheLine cacheSet[CACHE_LINE_CNT];
} cache;

void cache_init(cache *proxyCache);
/*
 *   Initialize the chache by setting values in every cache line to zero
 *   Initialize the cache fetching and writing semaphores.
 */

void cache_write(cache *proxyCache, unsigned long tag, unsigned int contentLength,
                 char *responseHeaders, void *responseContent);
/*
 *   Check if the size of the response is less than or equal to the MAX_OBJECT_SIZE.
 *   If so write it to the cache or return immediately other ways.
 */

int cache_fetch(cache *proxyCache, unsigned long *ptag, char *requestLine, char *requestHeaders, unsigned int *contentLength,
                char **responseHeaders, void **responseContent);
/*
 *   Check if the request is cached and serve the rseponse.
 *   return 0 if cached, -1 other ways.
 */

#endif