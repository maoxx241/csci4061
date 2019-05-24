#include <pthread.h>
#include "request.h"

extern int qlen;
static int head = 0;
static int tail = 0;
static request_t* request_queue[MAX_QUEUE_LEN] = {0};
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_full_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t queue_empty_cond = PTHREAD_COND_INITIALIZER;

static int is_empty()
{
    return head == tail;
}

static int is_full()
{
    return (tail+1)%MAX_QUEUE_LEN == head;
}

void enqueue_request(request_t* request)
{
    pthread_mutex_lock(&queue_mutex);
    while (is_full()) {
        pthread_cond_wait(&queue_full_cond, &queue_mutex);
    }

    request_queue[tail] = request;
    tail = (tail+1) % qlen;
    pthread_mutex_unlock(&queue_mutex);

    pthread_cond_signal(&queue_empty_cond);
}


request_t* dequeue_request()
{
    request_t* ret;
    pthread_mutex_lock(&queue_mutex);
    while (is_empty()) {
        pthread_cond_wait(&queue_empty_cond, &queue_mutex);
    }

    ret = request_queue[head];
    head = (head+1)%qlen;
    pthread_mutex_unlock(&queue_mutex);

    pthread_cond_signal(&queue_full_cond);
    return ret;
}