#include <stdio.h>
#include <pthread.h>
#include "logger.h"

#define LOG_FILE "web_server_log"

static FILE*                fp = NULL;
static pthread_mutex_t      fp_mutex = PTHREAD_MUTEX_INITIALIZER;

void log_request(int tid, int req_num, int fd, char* filename, int bytes, int service_time, int is_hit)
{
    pthread_mutex_lock(&fp_mutex);
    if (fp == NULL) {
        fp = fopen(LOG_FILE, "w+");
    }

    if (fp != NULL) {
        fprintf(fp, "[%d][%d][%d][%s][%d][%dms][%s]\n", 
            tid, req_num, fd, filename, bytes, service_time, is_hit?"HIT":"MISS");
        fflush(fp);
    }
    pthread_mutex_unlock(&fp_mutex);
}

void log_error(int tid, int req_num, int fd, char* filename, char* msg)
{
    pthread_mutex_lock(&fp_mutex);
    if (fp == NULL) {
        fp = fopen(LOG_FILE, "w+");
    }

    if (fp != NULL) {
        fprintf(fp, "[%d][%d][%d][%s][%s]\n", tid, req_num, fd, filename, msg);
        fflush(fp);
    }
    pthread_mutex_unlock(&fp_mutex);
}