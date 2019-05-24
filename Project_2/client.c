#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include "comm.h"
#include "util.h"

/* -------------------------Main function for the client ----------------------*/
int main(int argc, char * argv[]) {

  if (argc < 3) {
    printf("Usage: %s server id user id\n", argv[0]);
    return -1;
  }
	int pipe_user_reading_from_server[2], pipe_user_writing_to_server[2];

	// You will need to get user name as a parameter, argv[1].

	if(connect_to_server(argv[1], argv[2], pipe_user_reading_from_server, pipe_user_writing_to_server) == -1) {
		exit(-1);
	}

	/* -------------- YOUR CODE STARTS HERE -----------------------------------*/

  close(pipe_user_reading_from_server[1]);
  close(pipe_user_writing_to_server[0]);
  int r_fd = pipe_user_reading_from_server[0];
  int w_fd = pipe_user_writing_to_server[1];
  set_noblock(0);
  set_noblock(w_fd);
  set_noblock(r_fd);


	// poll pipe retrieved and print it to sdiout

	// Poll stdin (input from the terminal) and send it to server (child process) via pipe
  print_prompt(argv[2]);
  char user_id[MAX_USER_ID];
  while (1) {
    int nbytes;
    char buf[MAX_MSG+1];
    nbytes = read(0, buf, MAX_MSG);
    buf[nbytes] = '\0';
    if (nbytes != -1) {
      // printf("nbytes = %d\n", nbytes);
      trim_tail(buf);
      if (strlen(buf) == 0) {
        print_prompt(argv[2]);
        continue;
      }
      // printf("buf: %s %lu %d\n", buf, strlen(buf), nbytes);
      // fflush(stdin);
      enum command_type cmd;
      cmd = get_command_type(buf);
      switch (cmd) {
        case LIST:
        case EXIT:
        case BROADCAST:
          write(w_fd, buf, strlen(buf) + 1);
          break;
        case P2P:
          write(w_fd, buf, strlen(buf) + 1);
          break;
        default:
          fprintf(stderr, "Error commands\n");
          print_prompt(argv[2]);
          break;
      }
    }
    // print_prompt(argv[2]);
    nbytes = read(r_fd, buf, MAX_MSG);
    if (nbytes != -1 && nbytes != 0) {
      buf[nbytes] = '\0';
      if (strcmp(buf,"Invalid same user name")==0){
        printf("%s\n",buf);
        fflush(stdin);
        exit(1);
      }
      else if (strcmp(buf,"Out of space for user")==0){
        printf("%s\n",buf);
        fflush(stdin);
        exit(1);
      }
      else if (strlen(buf) != 0) {
        printf("%s\n",buf);
        fflush(stdin);
      }


      print_prompt(argv[2]);
    }
    if (nbytes == 0) {
      break;
    }
    usleep(1000);
  }
}

/*--------------------------End of main for the client --------------------------*/
