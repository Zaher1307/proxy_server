#include <semaphore.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "cache.h"

static unsigned long
generate_tag(const char *request_line, const char *request_headers);

static int
find_empty_line(Cache *cache);

static int
find_and_distruct_victim(Cache *cache);

static int
find_line(Cache *cache, unsigned int tag);

void
cache_init(Cache *cache)
{
    cache->readcnt = 0;
    cache->highest_timestamp = 0;
    sem_init(&cache->write_mutex, 0, 1);
    sem_init(&cache->readcnt_mutex, 0, 1);
    sem_init(&cache->timestamp_mutex, 0, 1);
    memset(cache->cache_set, 0, sizeof(cache->cache_set));
}

void
cache_write(Cache *cache, const char *request_line, const char *request_headers,
            const char *response_line, const char *response_headers, 
            const void *content, const size_t content_length)
{
    int index;
    unsigned int tag; 
    size_t object_size;
    char *response_line_ptr, *response_headers_ptr;
    void *content_ptr;

    /* check if the object_size can be fit in the cache line */
    object_size = content_length + strlen(response_line) 
                  + strlen(response_headers);
    if (object_size > MAX_OBJECT_SIZE)
        return;

    /* Create copy to be cached */
    tag = generate_tag(request_line, request_headers);

    /* 
     * Allocate for response content to avoid allocation overhead while
     * acquiring the mutex 
     */
    response_line_ptr = strdup(response_line);
    response_headers_ptr = strdup(response_headers);
    content_ptr = malloc(content_length);
    memcpy(content_ptr, content, content_length);

    /* Acquire the the write mutex to protect writing process */
    sem_wait(&cache->write_mutex);

    /* Find empty cache line */
    index = find_empty_line(cache);
    if (index < 0)
        index = find_and_distruct_victim(cache); // misleading name
    
    /* Place cache line */
    cache->cache_set[index].valid = 1;
    cache->cache_set[index].tag = tag;
    cache->cache_set[index].timestamp = ++cache->highest_timestamp;
    cache->cache_set[index].content_length = content_length;
    cache->cache_set[index].response_line = response_line_ptr;
    cache->cache_set[index].response_headers = response_headers_ptr;
    cache->cache_set[index].content = content_ptr;

    /* Release the write_mutex */
    sem_post(&cache->write_mutex);
}

int
cache_fetch(Cache *cache, const char *request_line, const char *request_headers,
            char **response_line, char **response_headers, void **content,
            size_t *content_length)
{
    unsigned int tag;
    int index, is_cached;

    tag = generate_tag(request_line, request_headers);
    sem_wait(&cache->readcnt_mutex);
    cache->readcnt++;
    if (cache->readcnt == 1)    /* First in */
        sem_wait(&cache->write_mutex);
    sem_post(&cache->readcnt_mutex);

    index = find_line(cache, tag);
    if (index < 0) {
        is_cached = 0;
    } else {
        is_cached = 1;
        *response_line = strdup(cache->cache_set[index].response_line);
        *response_headers = strdup(cache->cache_set[index].response_headers);
        
        *content = malloc(cache->cache_set[index].content_length);
        memcpy(*content, cache->cache_set[index].content, 
               cache->cache_set[index].content_length);
        *content_length = cache->cache_set[index].content_length;

        sem_wait(&cache->timestamp_mutex);
        cache->cache_set[index].timestamp = ++cache->highest_timestamp;
        sem_post(&cache->timestamp_mutex);
    }

    
    sem_wait(&cache->readcnt_mutex);
    cache->readcnt--;
    if (cache->readcnt == 0)    /* Last out */
        sem_post(&cache->write_mutex);
    sem_post(&cache->readcnt_mutex);

    return is_cached;
}

static unsigned long
generate_tag(const char *request_line, const char *request_headers)
{
    size_t len = strlen(request_line) + strlen(request_headers);
    unsigned long hash = 5381;
    char *hash_str;

    /* Build the string that will be hashed */
    hash_str = (char *) malloc(len + 1);
    hash_str[0] = '\0';
    strcat(hash_str, request_line);
    strcat(hash_str, request_headers);
    
    for (int i = 0; i < len; i++)
        hash = ((hash << 5) + hash) + hash_str[i]; /* hash * 33 + c */
    
    free(hash_str);
    return hash;
}

static int
find_empty_line(Cache *cache)
{
    for (int i = 0; i < CACHE_LINES; i++) {
        if (cache->cache_set[i].valid) 
            return i;
    }

    return -1;
}

static int
find_and_distruct_victim(Cache *cache)
{
    int index, least_recent_used;

    index = 0;
    least_recent_used = cache->cache_set[0].timestamp;
    for (int i = 1; i < CACHE_LINES; i++) {
        if (cache->cache_set[i].timestamp < least_recent_used) {
            index = i;
            least_recent_used = cache->cache_set[i].timestamp;
        }
    }

    /* Free the memory used by the victim line */
    // safe_free
    free(cache->cache_set[index].response_line);
    free(cache->cache_set[index].response_headers);
    free(cache->cache_set[index].content);

    return index;
}

static int
find_line(Cache *cache, unsigned int tag)
{
    for (int i = 0; i < CACHE_LINES; i++) {
        if (cache->cache_set[i].valid && cache->cache_set[i].tag == tag)
            return i;
    }

    return -1;
}

