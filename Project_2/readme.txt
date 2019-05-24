/* CSci4061 F2018 Assignment 2
* section: 1 and 2
* date: 11/10/2018
* name: Qi Mao, Matthew Warns,Yihan Zhou
* id:  */
The program make connections from server and user by create an new child process each time a new user add in.
And the user command can write to server through pipe communication.
Qi Mao does the basic structure of server programs and command user program.
Yihan Zhou does the error handling of use cases and server cases.
Matthew does the user program
to compile the program . type make in the command line in the folder first and ./server server_name to create a server
type ./client server_name client_name to create a new user connected to the server_fd
The main strategies for error handling is experiment with different cases
