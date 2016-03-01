#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <dyn_array.h>
#include <stdio.h>
#include "../include/processing_scheduling.h"

#define QUANTUM 4 // Used for Robin Round for process as the run time limit

//global lock variable
pthread_mutex_t mutex;

// private function
void virtual_cpu(ProcessControlBlock_t* process_control_block) {
	// decrement the burst time of the pcb
	sleep(1);
	--process_control_block->remaining_burst_time;
}

bool first_come_first_serve(dyn_array_t* ready_queue, ScheduleResult_t* result) {
    if (! ready_queue || ! result)
    {
        return false;
    }

    //setup queue
    ProcessControlBlock_t pcb;

    //Prep statistics calculations
    int numProcesses = 0;
    result->average_latency_time = 0.0f;
    result->average_wall_clock_time = 0.0f;
    result->total_run_time = 0;

    //keep looping around until all work is completed
    pthread_mutex_lock(&mutex);
    while (dyn_array_empty(ready_queue) == false)
    {
        result->average_latency_time += result->total_run_time;
        numProcesses++;

        //crank through another process
        dyn_array_extract_back(ready_queue, &pcb);
        pthread_mutex_unlock(&mutex);

        //process the block
        //store the fact that the process has started
        pcb.started = 1;

        while (pcb.remaining_burst_time)
        {
            //cycle through
            virtual_cpu(&pcb);
            result->total_run_time++;
        }

        result->average_wall_clock_time += result->total_run_time;
        pthread_mutex_lock(&mutex);
    }

    //done with mutex
    pthread_mutex_unlock(&mutex);

    //finished running all processes
    //divide out to find averages
    result->average_latency_time /= numProcesses;
    result->average_wall_clock_time /= numProcesses;

	return true;
}

void destroy_mutex (void) {
	pthread_mutex_destroy(&mutex);
};

// init the protected mutex
bool init_lock(void) {
	if (pthread_mutex_init(&mutex,NULL) != 0) {
		return false;
	}
	atexit(destroy_mutex);
	return true;
}

bool round_robin(dyn_array_t* ready_queue, ScheduleResult_t* result) {
    if (! ready_queue || ! result) {
        return false;
    }

    //setup queue
    ProcessControlBlock_t pcb;

    //Prep statistics calculations
    int numProcesses = 0;
    result->average_latency_time = 0.0f;
    result->average_wall_clock_time = 0.0f;
    result->total_run_time = 0;

    //run until empty
    pthread_mutex_lock(&mutex);
    while (dyn_array_empty(ready_queue) == false)
    {
        //extract last item in queue
        dyn_array_extract_back(ready_queue, &pcb);
        pthread_mutex_unlock(&mutex);

        //set that it has started if haven't done so already
        if (! pcb.started)
        {
            numProcesses++;
            pcb.started = 1;
            result->average_latency_time += result->total_run_time;
        }

        //process for quantum q or until done
        int timeLeft = QUANTUM;

        while (timeLeft && pcb.remaining_burst_time != 0)
        {
            //keep going
            virtual_cpu(&pcb);
            timeLeft--;
            result->total_run_time++;
        }

        //if task is completed
        if (pcb.remaining_burst_time == 0)
        {
            result->average_wall_clock_time += result->total_run_time;
        }
        else
        {
            //else, add the task back
            pthread_mutex_lock(&mutex);
            dyn_array_push_front(ready_queue, &pcb);
            pthread_mutex_unlock(&mutex);
        }

        pthread_mutex_lock(&mutex); //prepare for while statement
    }

    pthread_mutex_unlock(&mutex);

    //finished running all processes
    //divide out to find averages
    result->average_latency_time /= numProcesses;
    result->average_wall_clock_time /= numProcesses;

	return true;
}

/*
* MILESTONE 3 CODE
*/
dyn_array_t* load_process_control_blocks (const char* input_file ) {

    if (! input_file) {
        return NULL;
    }

    //try and open file
    int file = open(input_file, O_RDONLY);

    if (file == -1) {
        //error happened
        return NULL;
    }

    //file opened successfully

    //allocate memory for reading
    dyn_array_t* da = dyn_array_create(0, sizeof(ProcessControlBlock_t), NULL);

    //allocate pcb memory space
    ProcessControlBlock_t pcb;

    //set start time for all pcb's
    pcb.started = 0;

    //read in file

    //do primer read to skip number of objects
    int32_t i = 0, numObj = 0;
    int bytesRead = read(file, &numObj, sizeof(int32_t));

    //now read in first burst time length
    bytesRead = read(file, &i, sizeof(int32_t));

    //remove one object to read
    numObj--;

    while (bytesRead == sizeof(int32_t)) {

        //create a pcb and add to list
        pcb.remaining_burst_time = i;

        if (dyn_array_push_back(da, &pcb) == false) {
            dyn_array_destroy(da);
            close(file);
            return NULL; //failed to allocate enough space
        }

        bytesRead = read(file, &i, sizeof(int32_t));
        numObj--;
    }

    if (numObj > 0) {
        dyn_array_destroy(da);
        close(file);
        return NULL;
    }

    //validate the read worked
    if (bytesRead == -1) {
        //error reading file
        close(file);
        dyn_array_destroy(da);
    }

    //close file
    close(file);

    //check to see if valid number of bytes read
    if (bytesRead != 0 && bytesRead != sizeof(int32_t)) {
        //invalid binary file inputted
        //cleanup and return NULL
        dyn_array_destroy(da);
        return NULL;
    }

    //validate that there is at least one process to run
    if (dyn_array_size(da) == 0) {
        //invalid size detected
        dyn_array_destroy(da);
        return NULL;
    }

    //else good to go so return!
    return da;
}

void* first_come_first_serve_worker (void* input) {

    //validate input
    if (! input) {
        return NULL;
    }

    //cast input
    WorkerInput_t* data = (WorkerInput_t *)input;

    //validate data
    if (! data->ready_queue || ! data->result) {
        //these must be allocated
        return NULL;
    }

    //execute functionality
    first_come_first_serve(data->ready_queue, data->result);

    //return successful result!
    return NULL;
}

void* round_robin_worker (void* input) {

    //validate input
    if (! input) {
        return NULL;
    }

    //cast input
    WorkerInput_t* data = (WorkerInput_t *)input;

    //validate data
    if (! data->ready_queue || ! data->result) {
        //these must be allocated
        return NULL;
    }

    //execute functionality
    round_robin(data->ready_queue, data->result);

    //return successful result!
    return NULL;
}
