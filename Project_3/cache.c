#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "cache.h"


extern int cache_len;
cache_entry_t*      caches[MAX_CACHE_SIZE] = {NULL};
pthread_mutex_t     cache_mutex = PTHREAD_MUTEX_INITIALIZER;

void cache_lock()
{
    pthread_mutex_lock(&cache_mutex);
}

void cache_unlock()
{
    pthread_mutex_unlock(&cache_mutex);
}

void add_cache(cache_entry_t* cache)
{
    int i=0, idx = 0;
    int max_diff = 0;
    time_t now = time(NULL);
    for (int i=0; i<cache_len; ++i) {
        if (caches[i] == NULL) {
            caches[i] = cache;
            return;
        }

        if ((now-caches[i]->access_time) > max_diff) {
            idx = i;
            max_diff = now-caches[i]->access_time;
        }
    }

    /* replace old cache */
    free(caches[idx]->content);
    free(caches[idx]->content_type);
    free(caches[idx]);
    caches[idx] = cache;
    cache->access_time = time(NULL);
}

cache_entry_t* get_cache(char* filename)
{
    int i;
    for (i=0; i<cache_len; ++i) {
        if (caches[i] && strcmp(filename, caches[i]->filename) == 0) {
            caches[i]->access_time = time(NULL);
            return caches[i];
        }
    }

    return NULL;
}