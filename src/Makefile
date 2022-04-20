# Makefile for Proxy Lab 
#
# You may modify this file any way you like (except for the handin
# rule). You instructor will type "make" on your specific Makefile to
# build your proxy from sources.

# Makefile for Proxy Lab 

CC = gcc
CFLAGS = -g -Wall
LDFLAGS = -lpthread

PROXY = proxy.c
RIO = $(wildcard robust_input_output/rio.c)
SOCKET_HELPERS = $(wildcard socket_helpers/socket_helpers.c)
PROXY_SERVE = $(wildcard proxy_serve/proxy_serve.c)
PROXY_CACHE = $(wildcard proxy_cache/proxy_cache.c)

all: proxy

rio.o: $(RIO)
	$(CC) $(CFLAGS) -c $(RIO)

socket_helpers.o: $(SOCKET_HELPERS)
	$(CC) $(CFLAGS) -c $(SOCKET_HELPERS)

proxy_serve.o: $(PROXY_SERVE)
	$(CC) $(CFLAGS) -c $(PROXY_SERVE)

proxy_cache.o: $(PROXY_CACHE)
	$(CC) $(CFLAGS) -c $(PROXY_CACHE)

proxy.o: $(PROXY)
	$(CC) $(CFLAGS) -c $(PROXY)

proxy: proxy_serve.o rio.o socket_helpers.o proxy_cache.o proxy.o
	$(CC) $(CFLAGS) proxy_serve.o rio.o socket_helpers.o proxy_cache.o proxy.o -o proxy $(LDFLAGS)

clean:
	rm -f *~ *.o proxy
