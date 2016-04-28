#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <semaphore.h>
#include <error.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <time.h>
#include <mqueue.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>
#include <sys/shm.h>

#define DATA_SIZE 256
#define BUFF_SIZE 4096
#define NUM_SENDS BUFF_SIZE / DATA_SIZE

//define the message structure for the MQ
typedef struct mq_data {
    long mtype; //0 == last message, 1 == actual data
    char data[256];
} mq_data_t;

//Frees a shared memory segment
//Fail: Returns -1
//Success: Returns 0
int free_sm(int shmid)
{
    int res = shmctl(shmid, IPC_RMID, NULL);

    if (res == -1)
    {
        printf("Failed to free shared memory.\n");
        return -1;
    }

    return 0;
}

int main(void) {
    // seed the random number generator
    srand(time(NULL));

    // Parent and Ground Truth Buffers
    char ground_truth[BUFF_SIZE]    = {0};  // used to verify
    char producer_buffer[BUFF_SIZE] = {0};  // used by the parent

    // init the ground truth and parent buffer
    for (int i = 0; i < BUFF_SIZE; ++i) {
        producer_buffer[i] = ground_truth[i] = rand() % 256;
    }

//    // System V IPC keys for you to use
    const key_t s_msq_key = 1337;  // used to create message queue ipc
    //const key_t s_shm_key = 1338;  // used to create shared memory ipc
    //const key_t s_sem_key = 1339;  // used to create semaphore ipc
//    // POSIX IPC keys for you to use
//    const char *const p_msq_key = "OS_MSG";
//    const char *const p_shm_key = "OS_SHM";
//    const char *const p_sem_key = "OS_SEM";

    int id;

    /*
    * MESSAGE QUEUE SECTION
    **/

    printf("\n\nMESSAGE QUEUE SECTION\n\n");

    {
        //block this section off

        //first setup the messages q
        int msqid = msgget(s_msq_key, 0666 | IPC_CREAT);

        //now split up the process and have them execute different code
        id = fork();

        if (id == -1)
        {
            printf("Failed to fork process!\n");
            //close mq
            return -1;
        }

        if (id != 0)
        {
            //parent

            //send data

            //this pointer will reference the current position within the buffer
            char *ptr = producer_buffer;
            mq_data_t package; //this will hold the chunks of data to send over

            for (int j = 0; j < NUM_SENDS; ++j, ptr += DATA_SIZE)
            {
                //package the data
                package.mtype = 1;
                memcpy(package.data, ptr, DATA_SIZE);

                //send the data
                msgsnd(msqid, &package, sizeof(mq_data_t), 0);
            }

            //let the child know we're done sending information
            printf("Sending kill code.\n");
            package.mtype = 2;
            msgsnd(msqid, &package, sizeof(mq_data_t), 0);

            //wait for child to receive everything
            printf("Parent: Awaiting Pipe Section Child Complete.\n");
            waitpid(id, NULL, 0);
            msgctl(msqid, IPC_RMID, NULL); //clean this up
            printf("Parent: Message Queue Exit.\n");
        }
        else
        {
            //child

            //setup data to read
            char *ptr = ground_truth;
            mq_data_t data;

            //wait for parent to send everything and collect as it comes in
            for ( ; ; )
            {
                //read in a message
                int res = msgrcv(msqid, &data, sizeof(mq_data_t), 0, 0);

                if (res != -1)
                {
                    //managed to read something in!

                    //copy into ground_truth
                    if (data.mtype == 2)
                    {
                        //at the end of messages
                        break;
                    }

                    memcpy(ptr, data.data, DATA_SIZE);
                    ptr += DATA_SIZE;
                }

            }

            //validate success
            if (memcmp(ground_truth, producer_buffer, BUFF_SIZE) == 0)
            {
                printf("Succeeded in transferring data!\n");
            }
            else
            {
                printf("Data Invalid!\n");

                //first hundred in received
                for (int k = 0; k < 100; ++k)
                {
                    printf("%d,", ground_truth[k]);
                }
                printf("\n");

                //first hundred in original
                for (int k = 0; k < 100; ++k)
                {
                    printf("%d,", producer_buffer[k]);
                }
                printf("\n");
            }

            //return to parent
            printf("Child: Message Queue Finished.\n");
            _exit(0);
        }
    }

    /*
    * PIPE SECTION
    **/

    printf("\n\nPIPE SECTION\n\n");

    {
        //create the pipe
        int ends[2];
        if (pipe(ends) == -1)
        {
            printf("Failed to create the pipe.\n");
            exit(1);
        }


        //split into two processes
        id = fork();

        if (id == -1)
        {
            printf("Failed to fork process!\n");
            return -2;
        }

        if (id != 0)
        {
            //parent

            //prep communication
            close(ends[0]); //don't need to read
            char *ptr = producer_buffer;

            //send information
            for (int j = 0; j < NUM_SENDS; ++j, ptr += DATA_SIZE)
            {
                write(ends[1], ptr, DATA_SIZE);
            }

            close(ends[1]);

            //wait for child to receive everything
            printf("Parent: Awaiting Pipe Section Child Complete.\n");
            waitpid(id, NULL, 0);
            printf("Parent: Pipe Section Exit.\n");
        }
        else
        {
            //child

            //receive information
            char *ptr = ground_truth;

            //don't need to write
            close(ends[1]);

            for ( ; ; ptr += DATA_SIZE)
            {
                if (read(ends[0], ptr, DATA_SIZE) == 0)
                {
                    //the pipe has been closed
                    //basically no more information will be sent over
                    break;
                }
            }

            //close other end of pipe
            close(ends[0]);

            //validate success
            if (memcmp(ground_truth, producer_buffer, BUFF_SIZE) == 0)
            {
                printf("Succeeded in transferring data!\n");
            }
            else
            {
                printf("Data Invalid!\n");

                //first hundred in received
                for (int k = 0; k < 100; ++k)
                {
                    printf("%d,", ground_truth[k]);
                }
                printf("\n");

                //first hundred in original
                for (int k = 0; k < 100; ++k)
                {
                    printf("%d,", producer_buffer[k]);
                }
                printf("\n");
            }

            //end when everything transferred
            printf("Child: Pipe Section Completed.\n");
            _exit(0);
        }
    }

    /*
    * SHARED MEMORY AND SEMAPHORE SECTION
    **/

    printf("\n\nSHARED MEMORY AND SEMAPHORE SECTION\n\n");

    {
        //setup communication

        typedef struct data_shared
        {
            bool readyToRead;
            bool isLast;
            char data[DATA_SIZE];
            sem_t semLock;
        } data_shared_t;

        //Prep memory region
        data_shared_t *data;
        int sharedMemoryID;

        //instantiate shared memory
        if ((sharedMemoryID = shmget(IPC_PRIVATE, sizeof(data_shared_t), (S_IRUSR | S_IWUSR))) == -1)
        {
            perror("Failed to create shared memory segment");
            return 1;
        }

        //map the memory to local memory variable for easy access
        if ((data = (data_shared_t *)shmat(sharedMemoryID, NULL, 0)) == (void *) -1)
        {
            //TODO: Cleanup
            perror("Failed to attach memory segment");
            return 1;
        }

        if (sem_init(&data->semLock, 1/*shared across processes*/, 1) == -1)
        {
            //TODO: Cleanup
            perror("Failed to initialize semaphore.\n");
            return 1;
        }

        //split process
        id = fork();

        if (id == -1)
        {
            printf("Failed to fork process!\n");
            //TODO; cleanup everything
            return -3;
        }

        if (id != 0)
        {
            //parent

            //setup communications
            char *ptr = producer_buffer;

            //send information
            for (int j = 0; j < NUM_SENDS; ++j, ptr += DATA_SIZE)
            {
                while (data->readyToRead)
                {
                    //spin
                }

                //lock
                while (sem_wait(&data->semLock) == -1)
                {
                    if(errno != EINTR)
                    {
                        fprintf(stderr, "Thread failed to lock semaphore\n");
                        return 1;
                    }
                }

                //write
                data->readyToRead = true;
                data->isLast = false;
                memcpy(data->data, ptr, DATA_SIZE);

                if (j == NUM_SENDS - 1)
                {
                    //on last one
                    data->isLast = true;
                }

                //unlock
                if (sem_post(&data->semLock) == -1)
                {
                    fprintf(stderr, "Thread failed to unlock semaphore\n");
                }
            }

            //wait for child
            printf("Parent: Waiting for Shared Memory child.\n");
            waitpid(id, NULL, 0);
            printf("Parent: Reaped Shared Memory Child.\n");

            //free up the shared memory

            //unlink
            if (shmdt((void *)data) == -1) {  /* shared memory detach */
                perror("Failed to destroy shared memory segment");
                return 1;
            }

            //free
            if (free_sm(sharedMemoryID) == -1)
            {
                printf("Failed to free shared memory.\n");
                return 1;
            }
        }
        else
        {
            //child

            //receive information
            char *ptr = ground_truth;

            for (int j = 0; j < NUM_SENDS; ++j, ptr += DATA_SIZE)
            {
                while (data->readyToRead == false)
                {
                    //spin
                }

                //lock
                while (sem_wait(&data->semLock) == -1)
                {
                    if(errno != EINTR)
                    {
                        fprintf(stderr, "Thread failed to lock semaphore\n");
                        return 1;
                    }
                }

                //read
                memcpy(ptr, data->data, DATA_SIZE);

                //mark for another write request
                data->readyToRead = false;

                //unlock
                if (sem_post(&data->semLock) == -1)
                {
                    fprintf(stderr, "Thread failed to unlock semaphore\n");
                    return 1;
                }
            }

            //validate success
            if (memcmp(ground_truth, producer_buffer, BUFF_SIZE) == 0)
            {
                printf("Succeeded in transferring data!\n");
            }
            else
            {
                printf("Data Invalid!\n");

                //first hundred in received
                for (int k = 0; k < 100; ++k)
                {
                    printf("%d,", ground_truth[k]);
                }
                printf("\n");

                //first hundred in original
                for (int k = 0; k < 100; ++k)
                {
                    printf("%d,", producer_buffer[k]);
                }
                printf("\n");
            }

            //end
            printf("Child: Shared Memory Completed.\n");
            _exit(0);
        }
    }

    return 0;
}
