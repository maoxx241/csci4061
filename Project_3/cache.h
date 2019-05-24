#ifndef __CACHE_H__
#define __CACHE_H__

#include <time.h>

#define MAX_CACHE_SIZE 100

typedef struct _cache_entry {
    char filename[1024];
    char* content_type;
    char* content;
    int   size;
    time_t access_time;
} cache_entry_t;

void cache_lock();

void cache_unlock();

void add_cache(cache_entry_t* cache);

cache_entry_t* get_cache(char* filename);

#endif // __CACHE_H__