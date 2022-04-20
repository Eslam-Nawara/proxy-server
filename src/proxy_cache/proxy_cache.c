#include <string.h>
#include <stdlib.h>
#include "../proxy_serve/proxy_serve.h"
#include "proxy_cache.h"

static unsigned long
generate_tag(const char *request_line, const char *request_hdrs);

static int
find_empty_line(cache *proxyCache);

static unsigned int
find_vectum_line(cache *proxyCache);

static int
find_line_by_tag(cache *proxyCache, unsigned long tag);

void cache_init(cache *proxyCache)
{
    proxyCache->highestTimeStamp = 0;
    memset(proxyCache->cacheSet, 0, sizeof(proxyCache->cacheSet));
    proxyCache->readCnt = 0;
    sem_init(&proxyCache->writeMutex, 0, 1);
    sem_init(&proxyCache->readCntMutex, 0, 1);
    sem_init(&proxyCache->teimeStampMutex, 0, 1);
}

void cache_write(cache *proxyCache, unsigned long tag, unsigned int contentLength,
                 char *responseHeaders, void *responseContent)
{
    int line;

    if (contentLength + strlen(responseHeaders) > MAX_OBJECT_SIZE)
        return;

    // sem_wait(&proxyCache->writeMutex);

    if ((line = find_empty_line(proxyCache)) < 0)
        line = find_vectum_line(proxyCache);

    proxyCache->cacheSet[line].valid = 1;
    proxyCache->cacheSet[line].tag = tag;
    proxyCache->cacheSet[line].timeStamp = proxyCache->highestTimeStamp;
    proxyCache->cacheSet[line].contentLength = contentLength;
    proxyCache->cacheSet[line].responseHeaders = strdup(responseHeaders);
    proxyCache->cacheSet[line].content = malloc((contentLength));

    memcpy(proxyCache->cacheSet[line].content, responseContent, contentLength);
    (proxyCache->highestTimeStamp)++;

    sem_post(&proxyCache->writeMutex);
}

int cache_fetch(cache *proxyCache, unsigned long *ptag, char *requestLine, char *requestHeaders, unsigned int *contentLength,
                char **responseHeaders, void **responseContent)
{
    int line;
    int ret = 0;

    *ptag = generate_tag(requestLine, requestHeaders);

    sem_wait(&proxyCache->readCntMutex);
    (proxyCache->readCnt)++;
    if (proxyCache->readCnt == 1)
        sem_wait(&proxyCache->writeMutex);
    sem_post(&proxyCache->readCntMutex);

    if ((line = find_line_by_tag(proxyCache, *ptag)) < 0)
    {
        ret = -1;
    }
    else
    {
        *contentLength = proxyCache->cacheSet[line].contentLength;
        *responseHeaders = strdup(proxyCache->cacheSet[line].responseHeaders);

        *responseContent = malloc(*contentLength);
        memcpy(*responseContent, proxyCache->cacheSet[line].content, *contentLength);
    }

    sem_wait(&proxyCache->readCntMutex);
    proxyCache->readCnt--;
    if (proxyCache->readCnt == 0)
        sem_post(&proxyCache->readCntMutex);
    sem_post(&proxyCache->readCntMutex);

    return ret;
}

static int
find_empty_line(cache *proxyCache)
{
    for (unsigned int i = 0; i < CACHE_LINE_CNT; i++)
    {
        if (!(proxyCache->cacheSet[i].valid))
            return i;
    }
    return -1;
}

static unsigned int
find_vectum_line(cache *proxyCache)
{
    unsigned int lruLine = 0;
    unsigned long long lrTimeStamp = proxyCache->cacheSet[0].timeStamp;

    for (unsigned int i = 1; i < CACHE_LINE_CNT; i++)
    {
        if (proxyCache->cacheSet[i].timeStamp < lrTimeStamp)
        {
            lrTimeStamp = proxyCache->cacheSet[i].timeStamp;
            lruLine = i;
        }
    }

    free(proxyCache->cacheSet[lruLine].responseHeaders);
    free(proxyCache->cacheSet[lruLine].content);

    return lruLine;
}

static int
find_line_by_tag(cache *proxyCache, unsigned long tag)
{
    for (unsigned int i = 0; i < CACHE_LINE_CNT; i++)
    {
        if (proxyCache->cacheSet[i].valid && proxyCache->cacheSet[i].tag == tag)
            return (int)i;
    }
    return -1;
}

static unsigned long generate_tag(const char *requestLine, const char *requestHeaders)
{
    size_t len = strlen(requestLine) + strlen(requestHeaders);
    unsigned long hash = 5381;
    char *hashStr;

    /* Build the string that will be hashed */
    hashStr = (char *)malloc(len + 1);
    hashStr[0] = '\0';
    strcat(hashStr, requestLine);
    strcat(hashStr, requestHeaders);

    for (int i = 0; i < len; i++)
        hash = ((hash << 5) + hash) + hashStr[i]; /* hash * 33 + c */

    free(hashStr);
    return hash;
}
