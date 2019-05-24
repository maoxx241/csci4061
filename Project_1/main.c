/* CSci4061 F2018 Assignment 1
* login: maoxx241
* date: 9/30/2018
* name: Qi Mao
* id: 5306940 */

// This is the main file for the code
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "util.h"

/*-------------------------------------------------------HELPER FUNCTIONS PROTOTYPES---------------------------------*/
void show_error_message(char * ExecName);
//Write your functions prototypes here
void show_targets(target_t targets[], int nTargetCount);

/**
 * Build a target and it's dependent targets.
 * @param id The target id.
 * @param targets The DAG array.
 * @param nTargetCount The target count of DAG graph.
 * @return 0 for no need to build, 1 for need to build.
 */
int build_target(int id, target_t targets[], int nTargetCount);

/**
 * Check whether the target is valid (It's in the DAG array or it's a file on disk).
 * If the target is invalid, the process exits with -1.
 * @param targetName The name of target
 * @param targets The DAG array.
 * @param nTargetCount The target count of DAG graph.
 * @return The id of target in DAG, or -1 for file on disk.
 */
int check_target_valid(char* targetName, target_t targets[], int nTargetCount);

/**
 * Check whether the target need to build.
 * @param target The target
 * @return 1 for TRUE, 0 for FALSE.
 */
int check_need_build(target_t* target);

/**
 * Exec build command
 * @param command The command to be exec.
 */
void build(char* command);
/*-------------------------------------------------------END OF HELPER FUNCTIONS PROTOTYPES--------------------------*/


/*-------------------------------------------------------HELPER FUNCTIONS--------------------------------------------*/

//This is the function for writing an error to the stream
//It prints the same message in all the cases
void show_error_message(char * ExecName)
{
	fprintf(stderr, "Usage: %s [options] [target] : only single target is allowed.\n", ExecName);
	fprintf(stderr, "-f FILE\t\tRead FILE as a makefile.\n");
	fprintf(stderr, "-h\t\tPrint this message and exit.\n");
	exit(0);
}

//Write your functions here

//Phase1: Warmup phase for parsing the structure here. Do it as per the PDF (Writeup)
void show_targets(target_t targets[], int nTargetCount)
{
	//Write your warmup code here
    for (int i = 0; i < nTargetCount; ++i)
    {
        printf("TargetName: %s\n", targets[i].TargetName);
        printf("DependencyCount: %d\n", targets[i].DependencyCount);
        printf("DependencyNames: ");
        int n = targets[i].DependencyCount;
        for (int j = 0; j < n; ++j)
        {
            printf("%s", targets[i].DependencyNames[j]);
            if (j != (n - 1))
            {
                printf(", ");
            }
        }
        printf("\n");
        printf("Command: %s\n\n", targets[i].Command);
    }
	
}

int build_target(int id, target_t targets[], int nTargetCount)
{
    target_t target = targets[id];  // The target to build.
    int dependencyCount = target.DependencyCount;

    for (int i = 0; i < dependencyCount; ++i)
    {
        // Check dependencies of the target.
        // If any dependency is invalid, the process exit with -1.
        int dId = check_target_valid(target.DependencyNames[i],
                                     targets, nTargetCount);
        if (dId < 0)
        {
            // The dependency is file on disk.
            continue;
        }

        build_target(dId, targets, nTargetCount);
    }

    // Check whether the target need to build.
    if (check_need_build(&target))
    {
        printf("%s\n", target.Command);
        build(target.Command);
        return 1;
    }

    return 0;
}

int check_target_valid(char* targetName, target_t targets[], int nTargetCount)
{
    int id = find_target(targetName, targets, nTargetCount);
    if (id < 0 && does_file_exist(targetName) < 0)
    {
        // Target isn't in DAG array or on disk.
        printf("target %s doesn't exist.\n", targetName);
        exit(-1);
    }

    return id;
}

int check_need_build(target_t* target)
{
    for (int i = 0; i < target->DependencyCount; ++i)
    {
        int cmp = compare_modification_time(target->TargetName,
                                            target->DependencyNames[i]);
        // cmp == 1 and cmp == 0 means both target file and dependency file exist,
        // and target file is not older than dependency file.
        if ((cmp != 1) && (cmp != 0))
        {
            return TRUE;
        }
    }

    // When dependency count is 0, means the target has no dependency.
    // For this situation, it need to build.
    return target->DependencyCount == 0;
}

void build(char* command)
{
    char* tokens[ARG_MAX];
    char cmd[ARG_MAX];
    memset(tokens, 0, sizeof(char) * ARG_MAX);
    memset(cmd, 0, sizeof(char) * ARG_MAX);
    memcpy(cmd, command, strlen(command));

    int n = parse_into_tokens(cmd, tokens, " ");
    if (n <= 0)
    {
        // Invalid command.
        exit(-1);
    }

    pid_t pid = fork();
    if (pid < 0)
    {
        printf("fork failed with error code=%d\n", errno);
        exit(-1);
    }
    else if (pid == 0)
    {
        // Child process.
        execvp(tokens[0], tokens);

        // exec return only when it failed.
        printf("exec failed with error code=%d\n", errno);
        exit(-1);

    }

    int status;
    // Wait child process to be done.
    wait(&status);
    // Child process exit with error.
    if (WEXITSTATUS(status) != 0)
    {
        printf("child exited with error code=%d\n", WEXITSTATUS(status));
        exit(-1);
    }
}
/*-------------------------------------------------------END OF HELPER FUNCTIONS-------------------------------------*/


/*-------------------------------------------------------MAIN PROGRAM------------------------------------------------*/
//Main commencement
int main(int argc, char *argv[])
{
    target_t targets[MAX_NODES];
    int nTargetCount = 0;

    /* Variables you'll want to use */
    char Makefile[64] = "Makefile";
    char TargetName[64];

    /* Declarations for getopt */
    extern int optind;
    extern char * optarg;
    int ch;
    char *format = "f:h";
    char *temp;

    //Getopt function is used to access the command line arguments. However there can be arguments which may or may not need the parameters after the command
    //Example -f <filename> needs a finename, and therefore we need to give a colon after that sort of argument
    //Ex. f: for h there won't be any argument hence we are not going to do the same for h, hence "f:h"
    while((ch = getopt(argc, argv, format)) != -1)
    {
        switch(ch)
        {
        case 'f':
            temp = strdup(optarg);
            strcpy(Makefile, temp);  // here the strdup returns a string and that is later copied using the strcpy
            free(temp);	//need to manually free the pointer
            break;

        case 'h':
        default:
            show_error_message(argv[0]);
            exit(1);
        }

    }

    argc -= optind;
    if(argc > 1)   //Means that we are giving more than 1 target which is not accepted
    {
        show_error_message(argv[0]);
        return -1;   //This line is not needed
    }

    /* Init Targets */
    memset(targets, 0, sizeof(targets));   //initialize all the nodes first, just to avoid the valgrind checks

    /* Parse graph file or die, This is the main function to perform the toplogical sort and hence populate the structure */
    if((nTargetCount = parse(Makefile, targets)) == -1)  //here the parser returns the starting address of the array of the structure. Here we gave the makefile and then it just does the parsing of the makefile and then it has created array of the nodes
        return -1;


    //Phase1: Warmup-----------------------------------------------------------------------------------------------------
    //Parse the structure elements and print them as mentioned in the Project Writeup
    /* Comment out the following line before Phase2 */
    // show_targets(targets, nTargetCount);
    //End of Warmup------------------------------------------------------------------------------------------------------
   
    /*
    * Set Targetname
    * If target is not set, set it to default (first target from makefile)
    */
    if(argc == 1)
        strcpy(TargetName, argv[optind]);    // here we have the given target, this acts as a method to begin the building
    else
        strcpy(TargetName, targets[0].TargetName);  // default part is the first target

    /*
    * Now, the file has been parsed and the targets have been named.
    * You'll now want to check all dependencies (whether they are
    * available targets or files) and then execute the target that
    * was specified on the command line, along with their dependencies,
    * etc. Else if no target is mentioned then build the first target
    * found in Makefile.
    */

    //Phase2: Begins ----------------------------------------------------------------------------------------------------
    /*Your code begins here*/

    int id = check_target_valid(TargetName, targets, nTargetCount);
    int ret = build_target(id, targets, nTargetCount);
    if (ret == 0)
    {
        printf("make4061: '%s' is up to date.\n", TargetName);
    }


    /*End of your code*/
    //End of Phase2------------------------------------------------------------------------------------------------------

    return 0;
}
/*-------------------------------------------------------END OF MAIN PROGRAM------------------------------------------*/
