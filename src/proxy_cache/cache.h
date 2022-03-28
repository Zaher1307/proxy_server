#ifndef CACHE_H
#define CACHE_H

#include <semaphore.h>
#include <stddef.h>
#include <string.h>
#include <sys/types.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE  1049000     /* 1MB total cache size */
#define MAX_OBJECT_SIZE 102400      /* 1KB cache object size */
#define CACHE_LINES (MAX_CACHE_SIZE / MAX_OBJECT_SIZE)

typedef struct cache_line {
    char *response_line, *response_headers;
    void *content;
    size_t content_length;
    unsigned long long timestamp;
    unsigned int tag;
    unsigned char valid;
} CacheLine;

typedef struct cache {
    CacheLine cache_set[CACHE_LINES];
    sem_t write_mutex, readcnt_mutex, timestamp_mutex;
    unsigned long long highest_timestamp;
    unsigned long long readcnt;
} Cache;

void
cache_init(Cache *cache);

void
cache_write(Cache *cache, const char *request_line, const char *request_headers,
            const char *response_line, const char *response_headers, 
            const void *content, const size_t content_length);

int
cache_fetch(Cache *cache, const char *request_line, const char *request_headers,
            char **response_line, char **response_headers, void **content,
            size_t *content_length);

#endif
