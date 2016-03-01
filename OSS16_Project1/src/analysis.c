// Put the code for your analysis program here!
#include "../include/processing_scheduling.h"
#include <string.h>
#include <pthread.h>
#include <stdio.h>

/*
PURPOSE:
    Counts the number of FCFS requests in the arguments and validates input
PARAMETERS:
    argv: The values of the arguments
    argc: The number of arguments
Returns:
    * An integer representing the number of FCFS requests detected
    * -1 if there is an error such as an invalid argument
*/
static int process_args(char** argv, int argc) {
    int numFCFS = 0;

    int i;
    for (i = 2; i < argc; ++i) {
        char* str = argv[i];

        if (strcmp(str, "FCFS") == 0) {
            //found a first come first serve worker
            numFCFS++;
        }
        else if (strcmp(str, "RR") != 0) {
            //the string is not a round robin! so report an error
            printf("Invalid worker type detected: %s\n", str);
            return -1;
        }
    }

    //notify of success
    return numFCFS;
}

int main(int argc, char** argv) {

    if (argc <= 2) {
        //not enough arguments
        printf("Not enough arguments!\n");
        return 1;
    }

    //read in arguments
    char* file = argv[1];

    int totalThreads = argc - 2; //subtract two - one for exe name and one for file name of PCBs
    int numFCFS = process_args(argv, argc);

    if (numFCFS == -1) {
        //error occurred
        printf("Invalid worker input detected!\n");
        return 1;
    }

    int numRR = totalThreads - numFCFS;

    //prep mutex
    init_lock();

    //create list of pthreads
    pthread_t* threads = (pthread_t *) malloc(totalThreads * sizeof(pthread_t));

    //create list of results
    ScheduleResult_t* results = (ScheduleResult_t *) malloc(totalThreads * sizeof(ScheduleResult_t));

    //create list of worker inputs
    WorkerInput_t* workerInputs = (WorkerInput_t *) malloc(totalThreads * sizeof(WorkerInput_t));

    //create queue to process
    dyn_array_t* da = load_process_control_blocks(file);

    if (da == NULL) {
        //had issues...
        printf("Dynamic Array Alloc. Failed\n");
        return 1;
    }

    //create threads
    int i;
    for (i = 0; i < totalThreads; ++i) {
        //load same dynamic array for every worker
        workerInputs[i].ready_queue = da;

        //load different results for every worker
        workerInputs[i].result = &results[i];

        int res = 0; //used to validate creation...

        if (i < numFCFS) {
            //create an FCFS worker...
            res = pthread_create(&threads[i], NULL, first_come_first_serve_worker, &workerInputs[i]);
        }
        else {
            //create a RR worker...
            res = pthread_create(&threads[i], NULL, round_robin_worker, &workerInputs[i]);
        }

        if (res != 0) {
            //couldn't create pthread...
            printf("Failed to create thread!\n");

            //wait for currently running threads to complete
            int j;
            for (j = 0; j < i - 1; ++j) {
                pthread_join(threads[j], NULL);
            }

            //cleanup memory and return
            free(threads);
            free(results);
            free(workerInputs);
            dyn_array_destroy(da);
            return 1;
        }
    }

    //wait for completed threads
    for (i = 0; i < totalThreads; ++i) {
        pthread_join(threads[i], NULL);
    }

    //cleanup
    free(threads);
    free(results);
    free(workerInputs);
    dyn_array_destroy(da);

	return 0;
}
