#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

#define DATA_SIZE 256
#define BUFF_SIZE 4096


int main(void) {
    // seed the random number generator
    srand(time(NULL));

    // Parent and Ground Truth Buffers
    char ground_truth[BUFF_SIZE]    = {0};  // used to verify
    char producer_buffer[BUFF_SIZE] = {0};  // used by the parent

    // init the ground truth and parent buffer
    for (int = 0; i < BUFF_SIZE; ++i) {
        producer_buffer[i] = ground_truth[i] = rand() % 256;
    }

    // System V IPC keys for you to use
    const key_t s_msq_key = 1337;  // used to create message queue ipc
    const key_t s_shm_key = 1338;  // used to create shared memory ipc
    const key_t s_sem_key = 1339;  // used to create semaphore ipc
    // POSIX IPC keys for you to use
    const char *const p_msq_key = "OS_MSG";
    const char *const p_shm_key = "OS_SHM";
    const char *const p_sem_key = "OS_SEM";

    /*
    * MESSAGE QUEUE SECTION
    **/
	int id = fork();

	if (id != 0)
	{
        //parent

        //wait for child to receive everything
        printf("Parent: Awaiting Pipe Section Child Complete.\n");
        waitpid(id, NULL, 0);
        echo ("Parent: Message Queue Exit.\n");
	}
	else
	{
        //child

        //wait for parent to receive everything

        //return to parent
        printf("Child: Message Queue Finished.\n");
        _exit(0);
	}

    /*
    * PIPE SECTION
    **/
	id = fork();

    if (id != 0)
    {
        //parent

        //prep communication

        //send information

        //wait for child to receive everything
        printf("Parent: Awaiting Pipe Section Child Complete.\n");
        waitpid(id, NULL, 0);
        printf("Parent: Pipe Section Exit.\n");
    }
    else
    {
        //child

        //receive information

        //end when everything transferred
        printf("Child: Pipe Section Completed.\n");
        _exit(0);
    }

    /*
    * SHARED MEMORY AND SEMAPHORE SECTION
    **/
	id = fork();

	if (id != 0)
	{
        //parent

        //setup communications

        //send information

        //wait for child
        printf("Parent: Waiting for Shared Memory child.\n");
        waitpid(id, NULL, 0);
        printf("Parent: Reaped Shared Memory Child.\n");
	}
	else
	{
        //child

        //receive information

        //end
        printf("Child: Shared Memory Completed.\n");
        _exit(0);
	}

    return 0;
}
