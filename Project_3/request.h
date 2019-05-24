#ifndef __REQUEST_H__
#define __REQUEST_H__

#define MAX_QUEUE_LEN (100+1)

typedef struct _request {
    int     fd;
    char    filename[1024];
} request_t;

void enqueue_request(request_t* request);

request_t* dequeue_request();




#endif 