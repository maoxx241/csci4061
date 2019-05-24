#ifndef __LOGGER_H__
#define __LOGGER_H__


void log_request(int tid, int req_num, int fd, char* filename, int bytes, int service_time, int is_hit);
void log_error(int tid, int req_num, int fd, char* filename, char* msg);

#endif 