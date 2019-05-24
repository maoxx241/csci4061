#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include "comm.h"
#include "util.h"

/* -----------Functions that implement server functionality -------------------------*/



/*
 * Returns the empty slot on success, or -1 on failure
 */
int find_empty_slot(USER * user_list) {
	// iterate through the user_list and check m_status to see if any slot is EMPTY
	// return the index of the empty slot
    int i = 0;
	for(i=0;i<MAX_USER;i++) {
    	if(user_list[i].m_status == SLOT_EMPTY) {
			return i;
		}
	}
	return -1;
}

/*
 * list the existing users on the server shell
 */
int list_users(int idx, USER * user_list)
{
	// iterate through the user list
	// if you find any slot which is not empty, print that m_user_id
	// if every slot is empty, print "<no users>""
	// If the function is called by the server (that is, idx is -1), then printf the list
	// If the function is called by the user, then send the list to the user using write() and passing m_fd_to_user
	// return 0 on success
	int i, flag = 0;
	char buf[MAX_MSG] = {}, *s = NULL;

	/* construct a list of user names */
	s = buf;
	strncpy(s, "---connecetd user list---\n", strlen("---connecetd user list---\n"));
	s += strlen("---connecetd user list---\n");
	for (i = 0; i < MAX_USER; i++) {
		if (user_list[i].m_status == SLOT_EMPTY)
			continue;
		flag = 1;
		strncpy(s, user_list[i].m_user_id, strlen(user_list[i].m_user_id));
		s = s + strlen(user_list[i].m_user_id);
		strncpy(s, "\n", 1);
		s++;
	}
	if (flag == 0) {
		strcpy(buf, "<no users>\n");
	} else {
		s--;
		strncpy(s, "\0", 1);
	}

	if(idx < 0) {
		printf(buf);
		printf("\n");
	} else {
		/* write to the given pipe fd */
		if (write(user_list[idx].m_fd_to_user, buf, strlen(buf) + 1) < 0)
			perror("writing to server shell"); }

	return 0;
}

/*
 * add a new user
 */
int add_user(int idx, USER * user_list, int pid, char * user_id, int pipe_to_child, int pipe_to_parent)
{
	// populate the user_list structure with the arguments passed to this function
	// return the index of user added
  user_list[idx].m_pid = pid;
  strcpy(user_list[idx].m_user_id, user_id);
  user_list[idx].m_fd_to_user = pipe_to_child;
  user_list[idx].m_fd_to_server = pipe_to_parent;
  user_list[idx].m_status = SLOT_FULL;
	return 0;
}

/*
 * Kill a user
 */
void kill_user(int idx, USER * user_list) {
	// kill a user (specified by idx) by using the systemcall kill()
	// then call waitpid on the user
  kill(user_list[idx].m_pid, SIGUSR1);
  int exit_status;
  waitpid(user_list[idx].m_pid, &exit_status, WNOHANG | WUNTRACED);
}

/*
 * Perform cleanup actions after the used has been killed
 */
void cleanup_user(int idx, USER * user_list)
{
	// m_pid should be set back to -1
	// m_user_id should be set to zero, using memset()
	// close all the fd
	// set the value of all fd back to -1
	// set the status back to empty
  user_list[idx].m_pid = -1;
  memset(user_list[idx].m_user_id, 0, MAX_USER_ID);
  close(user_list[idx].m_fd_to_user);
  close(user_list[idx].m_fd_to_server);
  user_list[idx].m_fd_to_user = -1;
  user_list[idx].m_fd_to_server = -1;
  user_list[idx].m_status = SLOT_EMPTY;
}

/*
 * Kills the user and performs cleanup
 */
void kick_user(int idx, USER * user_list) {
	// should kill_user()
	// then perform cleanup_user()
  kill_user(idx, user_list);
  cleanup_user(idx, user_list);
}

/*
 * broadcast message to all users
 */
int broadcast_msg(USER * user_list, char *buf, char *sender)
{
	//iterate over the user_list and if a slot is full, and the user is not the sender itself,
	//then send the message to that user
	//return zero on success
  int idx = 0;
  int n_send_msg = strlen(buf) + strlen(sender) + 3;
  char* send_msg = (char*)malloc(n_send_msg);
  strcpy(send_msg + 1, sender);
  strcpy(send_msg + 1 + strlen(send_msg + 1), ":");
  strcpy(send_msg + 1 + strlen(send_msg + 1), buf);
  for (idx = 0; idx < MAX_USER; ++idx) {
    if (user_list[idx].m_status != SLOT_EMPTY) {
      if (strcmp(user_list[idx].m_user_id, sender) != 0 || strcmp(sender, "Notice") == 0) {
        send_msg[0] = '\n';
        // printf("msg: %s\n", send_msg);
        if (write(user_list[idx].m_fd_to_user, send_msg, strlen(send_msg) + 1) == -1) {
          perror("broadcast_msg failed");
          return -1;
        }
      } else {
        if (write(user_list[idx].m_fd_to_user, send_msg + 1, strlen(send_msg + 1) + 1) == -1) {
          perror("broadcast_msg failed");
          return -1;
        }
      }
    }
  }
  free(send_msg);
  char msg[n_send_msg];
  strcpy(msg, sender);
  strcat(msg, ":");
  strcat(msg, buf);
  printf("\n");
  printf(msg);
  printf("\0");
  printf("\n");
  if (strcmp(sender,"Notice")!=0){
    print_prompt("admin");
  }
	return 0;
}

/*
 * Cleanup user chat boxes
 */
void cleanup_users(USER * user_list)
{
	// go over the user list and check for any empty slots
	// call cleanup user for each of those users.
  int idx = 0;
  for (idx = 0; idx < MAX_USER; ++idx) {
    if (user_list[idx].m_status != SLOT_EMPTY) {
      kill_user(idx, user_list);
    }
  }
}

/*
 * find user index for given user name
 */
int find_user_index(USER * user_list, char * user_id)
{
	// go over the  user list to return the index of the user which matches the argument user_id
	// return -1 if not found
	int i, user_idx = -1;

	if (user_id == NULL) {
		fprintf(stderr, "NULL name passed.\n");
		return user_idx;
	}
	for (i=0;i<MAX_USER;i++) {
		if (user_list[i].m_status == SLOT_EMPTY)
			continue;
		if (strcmp(user_list[i].m_user_id, user_id) == 0) {
			return i;
		}
	}

	return -1;
}

/*
 * given a command's input buffer, extract name
 */
int extract_name(char * buf, char * user_name)
{
	char inbuf[MAX_MSG];
    char * tokens[16];
    strcpy(inbuf, buf);

    int token_cnt = parse_line(inbuf, tokens, " ");

    if(token_cnt >= 2) {
        strcpy(user_name, tokens[1]);
        return 0;
    }

    return -1;
}

int extract_text(char *buf, char * text)
{
    char inbuf[MAX_MSG];
    char * tokens[16];
    char * s = NULL;
    strcpy(inbuf, buf);

    int token_cnt = parse_line(inbuf, tokens, " ");

    if(token_cnt >= 3) {
        //Find " "
        s = strchr(buf, ' ');
        s = strchr(s+1, ' ');

        strcpy(text, s+1);
        return 0;
    }

    return -1;
}

/*
 * send personal message
 */
void send_p2p_msg(int idx, USER * user_list, char *buf)
{
	// get the target user by name using extract_name() function
	// find the user id using find_user_index()
	// if user not found, write back to the original user "User not found", using the write()function on pipes.
	// if the user is found then write the message that the user wants to send to that user.
  char user_id[MAX_USER_ID];
  char send_msg[MAX_MSG];
  int flag = 0;
  if (extract_name(buf, user_id) == -1) {
    strcpy(send_msg, "User not found");
    flag = 1;
  }
  int send_idx = find_user_index(user_list, user_id);
  if (send_idx == -1) {
    strcpy(send_msg, "User not found");
    flag = 1;
  }

  if (flag == 1) {
    write(user_list[idx].m_fd_to_user, send_msg, strlen(send_msg) + 1);
  } else {
    if (extract_text(buf, send_msg) == -1) {
      fprintf(stderr, "Error Message\n");
      return;
    }
    if (strcmp(user_list[idx].m_user_id, user_id) != 0) {
      char new_msg[MAX_MSG + 1];
      new_msg[0] = '\n';
      strcpy(new_msg + 1, user_list[idx].m_user_id);
      strcat(new_msg," : ");
      strcat(new_msg, send_msg);
      write(user_list[send_idx].m_fd_to_user, new_msg, strlen(new_msg) + 1);
      write(user_list[idx].m_fd_to_user, "\0", 1);
    } else {
      write(user_list[send_idx].m_fd_to_user, send_msg, strlen(send_msg) + 1);
    }
  }
  printf("%s : %s  %s \n\0",user_list[idx].m_user_id,user_id,send_msg);
  print_prompt("admin");
}

//takes in the filename of the file being executed, and prints an error message stating the commands and their usage
void show_error_message(char *filename)
{
  printf("%s: error argument\n", filename);
  printf("Usage: %s YOUR_UNIQUE_ID\n", filename);
}


/*
 * Populates the user list initially
 */
void init_user_list(USER * user_list) {

	//iterate over the MAX_USER
	//memset() all m_user_id to zero
	//set all fd to -1
	//set the status to be EMPTY
	int i=0;
	for(i=0;i<MAX_USER;i++) {
		user_list[i].m_pid = -1;
		memset(user_list[i].m_user_id, '\0', MAX_USER_ID);
		user_list[i].m_fd_to_user = -1;
		user_list[i].m_fd_to_server = -1;
		user_list[i].m_status = SLOT_EMPTY;
	}
}

/* ---------------------End of the functions that implementServer functionality -----------------*/



int kick_by_user_name(USER* user_list, char* buf) {
  char user_id[MAX_USER_ID];
  if (extract_name(buf, user_id) == -1) {
    fprintf(stderr, "Error user name\n");
    return -1;
  }
  int idx = find_user_index(user_list, user_id);
  if (idx == -1) {
    fprintf(stderr, "Error user name\n");
    return -1;
  }
  kick_user(idx, user_list);
  return 0;
}


int process_cmd(int fd, USER* user_list, int idx, FILE* test) {
  int nbytes;
  char buf[MAX_MSG + 1];
  if (fd == -1) {
    fd = user_list[idx].m_fd_to_server;
  }
  nbytes = read(fd, buf, MAX_MSG);
  if (nbytes == -1) {
    return nbytes;
  }
  buf[nbytes] = '\0';
  trim_tail(buf);
  enum command_type cmd;
  cmd = get_command_type(buf);
  if (test != NULL) {
    fprintf(test, "\nbuf: %d %s %d\n", fd, buf, cmd);
    fflush(test);
  }
  switch (cmd) {
    case LIST:
      if (fd == 0) {
        list_users(-1, user_list);
      } else {
        list_users(idx, user_list);
      }
      break;
    case KICK:
      if (fd != 0) {
        fprintf(stderr, "Error commands\n");
        break;
      }
      kick_by_user_name(user_list, buf);
      break;
    case EXIT:
      if (fd == 0) {
        cleanup_users(user_list);
        exit(0);
      } else {
        kick_user(idx, user_list);
      }
      break;
    case BROADCAST:
      if (fd == 0) {
        broadcast_msg(user_list, buf, "Notice");
      } else {
        broadcast_msg(user_list, buf, user_list[idx].m_user_id);
      }
      break;
    case P2P:
      if (fd == 0) {
        fprintf(stderr, "Error commands\n");
        break;
      }
      send_p2p_msg(idx, user_list, buf);
      break;
    default:
      fprintf(stderr, "Error commands\n");
      break;
  }
  return nbytes;
}

/* ---------------------Start of the Main function ----------------------------------------------*/
int main(int argc, char * argv[])
{
  if (argc < 2) {
    show_error_message(argv[0]);
    return EXIT_FAILURE;
  }

  // FILE* test = fopen("test2.txt", "w+");
  // if (test == NULL) {
  //   exit(-1);
  // }

  int i;
  int idx;
	setup_connection(argv[1]); // Specifies the connection point as argument.

	USER user_list[MAX_USER];
	init_user_list(user_list);   // Initialize user list

  set_noblock(0);

	print_prompt("admin");
	while(1) {
		/* ------------------------YOUR CODE FOR MAIN--------------------------------*/

		// Handling a new connection using get_connection
    int pipe_child_writing_to_user[2];
    int pipe_child_reading_from_user[2];
		char user_id[MAX_USER_ID];
    int ret = get_connection(
        user_id,
        pipe_child_writing_to_user,
        pipe_child_reading_from_user);

    if (ret == 0) {
      // Check max user and same user id
      idx = find_user_index(user_list, user_id);
      if (idx != -1) {
        write(pipe_child_writing_to_user[1],"Invalid same user name",MAX_MSG);
        continue;
      }
      idx = find_empty_slot(user_list);
      if (idx == -1) {

        write(pipe_child_writing_to_user[1],"Out of space for user",MAX_MSG);
        continue;
      }

      int pipe_SERVER_reading_from_child[2];
      int pipe_SERVER_writing_to_child[2];
      if (pipe(pipe_SERVER_writing_to_child) < 0 || pipe(pipe_SERVER_reading_from_child) < 0) {
        perror("Failed to create pipe\n");
        continue;
      }

      // Child process: poli users and SERVER
      int pid = fork();
      if (pid == -1) {
        perror("fork failed");
        exit(-1);
      }
      if (pid == 0) { // child
        // printf("new user\n");
        close(pipe_SERVER_writing_to_child[1]);
        close(pipe_SERVER_reading_from_child[0]);
        close(pipe_child_writing_to_user[0]);
        close(pipe_child_reading_from_user[1]);
        int w_server_fd = pipe_SERVER_reading_from_child[1];
        int r_server_fd = pipe_SERVER_writing_to_child[0];
        int w_user_fd = pipe_child_writing_to_user[1];
        int r_user_fd = pipe_child_reading_from_user[0];
        set_noblock(w_server_fd);
        set_noblock(r_server_fd);
        set_noblock(w_user_fd);
        set_noblock(r_user_fd);

        while (1) {
          // printf("in child");
          int nbytes;
          char buf[MAX_USER + 1];
          nbytes = read(r_server_fd, buf, MAX_MSG);
          if (nbytes != -1) {
            buf[nbytes] = '\0';
            // printf("read from server %d %s\n", nbytes, buf);
            write(w_user_fd, buf, nbytes);
          }
          nbytes = read(r_user_fd, buf, MAX_MSG);
          if (nbytes != -1) {
            write(w_server_fd, buf, nbytes);
          }
          usleep(1000);
        }
      }


      // Server process: Add a new user information into an empty slot
      // poll child processes and handle user commands
      // Poll stdin (input from the terminal) and handle admnistrative command
      close(pipe_SERVER_writing_to_child[0]);
      close(pipe_SERVER_reading_from_child[1]);
      close(pipe_child_writing_to_user[0]);
      close(pipe_child_writing_to_user[1]);
      close(pipe_child_reading_from_user[0]);
      close(pipe_child_reading_from_user[1]);

      add_user(idx, user_list, pid, user_id,
          pipe_SERVER_writing_to_child[1], pipe_SERVER_reading_from_child[0]);
      set_noblock(pipe_SERVER_writing_to_child[1]);
      set_noblock(pipe_SERVER_reading_from_child[0]);

    }
    ret = process_cmd(0, user_list, -1, NULL);
    if (ret != -1) {
	    print_prompt("admin");
    }
    for (i = 0; i < MAX_USER; ++i) {
      if (user_list[i].m_status == SLOT_FULL) {
        process_cmd(-1, user_list, i, NULL);
      }
    }
    usleep(1000);
	}
}

/* --------------------End of the main function ----------------------------------------*/
