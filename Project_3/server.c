#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/time.h>
#include <linux/limits.h>
#include <time.h>
#include "util.h"
#include <stdbool.h>
#include <unistd.h>
#include "request.h"
#include "cache.h"
#include "logger.h"

#define MAX_THREADS 100
#define INVALID -1
#define BUFF_SIZE 1024
#define DYNAMIC_THREASHOLD 10

char* web_root_path;
int dynamic_flag, qlen, cache_len, num_dispatcher, num_workers;


typedef struct _worker {
  int         id; /* thread id */
  int         req_num;  /* number of requests handled */
  pthread_t   tid; /* tid */
  int         stop_flag;
  int         is_running;
} worker_t;

pthread_t dispatch_threads[MAX_THREADS];
worker_t* worker_threads[MAX_THREADS] = {NULL};

void* worker_routine(void *arg);

/* ************************ Dynamic Pool Code ***********************************/
// Extra Credit: This function implements the policy to change the worker thread pool dynamically
// depending on the number of requests
void * dynamic_pool_size_update(void *arg) {
  int check_interval = 60;
  int worker_records[MAX_THREADS] = {0};
  while(1) {
    int i;
    int num_idle_threads = 0;

    // Run at regular intervals
    sleep(check_interval);

    if (num_workers <= 1) {
      continue;
    }

    // clean up stopped threads last interval
    for (i=0; i<MAX_THREADS; ++i) {
      if (worker_threads[i] && worker_threads[i]->is_running == 0) {
        pthread_join(worker_threads[i]->tid, NULL);
        free(worker_threads[i]);
        worker_threads[i] = NULL;
      }
    }

    // check
    for (i=0; i<num_workers; ++i) {
      if (worker_threads[i]) {
        if (worker_threads[i]->req_num - worker_records[i] < DYNAMIC_THREASHOLD) {
          num_idle_threads++;
        }

        worker_records[i] = worker_threads[i]->req_num;
      }
    }

    if (num_idle_threads > num_workers / 2) {
      // decrease threads
      printf("Removed <%d> worker threads because threads process less than %d requests during the past %d seconds.\n", (num_workers - num_workers/2), DYNAMIC_THREASHOLD, check_interval);
      for (i=num_workers/2; i<num_workers; ++i) {
        worker_threads[i]->stop_flag = 1; /* stop */
        enqueue_request(NULL); /* notify */
        worker_records[i] = 0;
      }

      num_workers = num_workers - num_workers/2;

    } else if (num_idle_threads == 0) {
      // Increase more threads
      printf("Created <%d> worker threads because all threads processed more than %d requests during the past %d seconds\n", num_workers, DYNAMIC_THREASHOLD, check_interval);
      for (i=num_workers; i<num_workers*2; ++i) {
        worker_records[i] = 0;
        worker_t* worker = (worker_t*)malloc(sizeof(worker_t));
        worker->id = i;
        worker->stop_flag = 0;
        worker->req_num = 0;
        worker_threads[i] = worker;
        pthread_create(&(worker->tid), NULL, worker_routine, worker);
      }

      num_workers += num_workers;
    }
    
  }
}

// Function to receive the request from the client and add to the queue
void* dispatch_routine(void *arg) {

  while (1) {
    // Accept client connection
    int ret;
    request_t* request = (request_t*)malloc(sizeof(request_t));

    if ((request->fd = accept_connection()) < 0) {
      printf("accept_connection failed, ret:%d\n", request->fd);
      free(request);
      continue;
    }

    // Get request from the client
    if ((ret = get_request(request->fd, request->filename)) != 0) {
      // TODO: error 
      free(request);
      continue;
    } 

    // Add the request into the queue
      enqueue_request(request);
   }
   return NULL;
}

/**********************************************************************************/

// Function to retrieve the request from the queue, process it and then return a result to the client
void* worker_routine(void *arg) {

  worker_t* self = (worker_t*)arg;
  self->is_running = 1;
  
  while (!self->stop_flag) {
    int ret;
    char* content_type;
    int content_size = 0;
    char* content_buf = NULL;
    cache_entry_t* cache = NULL;

    // Start recording time
    struct timeval beg_time, end_time;
    gettimeofday(&beg_time, NULL);

    self->req_num++;
    // Get the request from the queue
    request_t* request = dequeue_request();
    if (request == NULL) {
      continue;
    }

    // Get the data from the disk or the cache
    cache_lock();
    cache = get_cache(request->filename);
    if (cache != NULL) {
      /* cache hit, use cache */
      content_size = cache->size;
      content_buf = (char*)malloc(content_size);
      content_type = strdup(cache->content_type);
      memcpy(content_buf, cache->content, content_size);
      cache_unlock();
    } else {
      /* cache miss, read from disk */
      char* pos;
      int bytes = 0;
      int file_size;
      FILE* fp = NULL;
      char fpath[PATH_MAX];
      cache_entry_t* cache_entry;
      cache_unlock();

      strcpy(fpath, web_root_path);
      strcat(fpath, request->filename);
      fp = fopen(fpath, "rb");
      if (fp == NULL) {
        return_error(request->fd, "File not found.");
        log_error(self->id, self->req_num, request->fd, request->filename, "File not found.");
        continue;
      }
      
      /* get file size */
      fseek(fp, 0, SEEK_END);
      file_size = ftell(fp);
      fseek(fp, 0, SEEK_SET); /* restore*/
      content_size = file_size;
      content_buf = (char*)malloc(file_size);
      /* get file type */
      pos = strrchr(request->filename, '.');
      if (pos == NULL) {
        content_type = "text/plain";
      } else {
        pos++;
        if (strcmp(pos, "html") == 0 || strcmp(pos, "htm") == 0) {
          content_type = "text/html";
        } else if(strcmp(pos, "jpg") == 0 || strcmp(pos, "jpeg") == 0) {
          content_type = "image/jpeg";
        } else if (strcmp(pos, "gif") == 0) {
          content_type = "image/gif";
        } else {
          content_type = "text/plain";
        }
      }
      /* get file content */
      while (!ferror(fp) && !feof(fp) && bytes < content_size) {
        bytes += fread(content_buf+bytes, sizeof(char), content_size-bytes, fp);
      }

      cache_entry = (cache_entry_t*)malloc(sizeof(cache_entry_t));
      strcpy(cache_entry->filename, request->filename);
      cache_entry->content_type = strdup(content_type);
      cache_entry->size = content_size;
      cache_entry->content = content_buf;

      cache_lock();
      add_cache(cache_entry);
      cache_unlock();
    }

    // Stop recording the time
    gettimeofday(&end_time, NULL);

    // Log the request into the file and terminal
    int service_time = ((end_time.tv_sec - beg_time.tv_sec)* 10e6 + (end_time.tv_usec - beg_time.tv_usec))/10e3;
    log_request(self->id, self->req_num, request->fd, request->filename, content_size, service_time, (cache != NULL));

    // return the result
    return_result(request->fd, content_type, content_buf, content_size);
    
    // free resource
    if (cache != NULL) {
      free(content_buf);
      free(content_type);
    }

    free(request);
  }

  self->is_running = 0;
  return NULL;
}

/**********************************************************************************/

int main(int argc, char **argv) {
  int i, ret, port;
  pthread_t dynamic_thread;
  // Error check on number of arguments
  if(argc != 8){
    printf("usage: %s port path num_dispatcher num_workers dynamic_flag queue_length cache_size\n", argv[0]);
    return -1;
  }

  // Get the input args & Perform error checks on the input arguments
  port = atoi(argv[1]);
  if (port < 1025 || port > 65535) {
    printf("invalid port number[1025, 65535]:%d\n", port);
    return -1;
  }

  web_root_path = argv[2];
  if (strlen(web_root_path) == 0 || access(web_root_path, F_OK) != 0) {
    // check if web_root_path exists
    printf("invalid web root path:%s\n", web_root_path);
    return -1;
  }

  num_dispatcher = atoi(argv[3]);
  if (num_dispatcher <= 0 || num_dispatcher > MAX_THREADS) {
    printf("invalid dispatcher num[1, %d]:%d\n", MAX_THREADS, num_dispatcher);
    return -1;
  }

  num_workers = atoi(argv[4]);
  if (num_workers <= 0 || num_workers > MAX_THREADS) {
    printf("invalid dispatcher num(0, %d]:%d\n", MAX_THREADS, num_workers);
    return -1;
  }

  dynamic_flag = atoi(argv[5]);
  if (dynamic_flag) {
    pthread_create(&dynamic_thread, NULL, dynamic_pool_size_update, NULL);
  }

  qlen = atoi(argv[6]);
  if (qlen <= 0 || qlen > MAX_QUEUE_LEN) {
    printf("invalid queue length(0, %d]: %d\n", MAX_QUEUE_LEN, qlen);
    return -1;
  }

  cache_len = atoi(argv[7]);
  if (cache_len < 0 || cache_len > MAX_CACHE_SIZE) {
    printf("invalid cache size[0, %d]: %d\n", MAX_CACHE_SIZE, cache_len);
    return -1;
  }

  // getcwd(web_root_path, sizeof(web_root_path));
  
  // Start the server and initialize cache
  init(port);
  
  
  // Create dispatcher and worker threads
  /* start work threads */
  for (i=0; i<num_workers; ++i) {
    worker_t* worker = (worker_t*)malloc(sizeof(worker_t));
    worker->id = i;
    worker->req_num = 0;
    worker->stop_flag = 0;
    worker->is_running = 0;
    worker_threads[i] = worker;
    pthread_create(&(worker->tid), NULL, worker_routine, worker);
  }
  /* start dispatch threads */
  for (i=0; i<num_dispatcher; ++i) {
    pthread_create(&dispatch_threads[i], NULL, dispatch_routine, NULL);
  }


  // Clean up
  if (dynamic_flag) {
    pthread_join(dynamic_thread, NULL);
  }

  for (i=0; i<num_dispatcher; ++i) {
    pthread_join(dispatch_threads[i], NULL);
  }

  for (i=0; i<num_workers; ++i) {
    pthread_join(worker_threads[i]->tid, NULL);
  }

  return 0;
}
