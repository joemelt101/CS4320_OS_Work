#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <dyn_array.h>
#include "../include/processing_scheduling.h"

#define QUANTUM 4 // Used for Robin Round for process as the run time limit

//global lock variable
pthread_mutex_t mutex;

// private function
void virtual_cpu(ProcessControlBlock_t* process_control_block) {
	// decrement the burst time of the pcb
	--process_control_block->remaining_burst_time;
	sleep(1);
}

bool first_come_first_serve(dyn_array_t* ready_queue, ScheduleResult_t* result) {
    if (! ready_queue || ! result)
    {
        return false;
    }

    //setup queue
    ProcessControlBlock_t *pcb;

    //Prep statistics calculations
    int numProcesses = dyn_array_size(ready_queue);
    result->average_latency_time = 0.0f;
    result->average_wall_clock_time = 0.0f;
    result->total_run_time = 0;

    //keep looping around until all work is completed
    while (dyn_array_empty(ready_queue) == false)
    {
        result->average_latency_time += result->total_run_time;

        //crank through another process
        pcb = (ProcessControlBlock_t *)dyn_array_back(ready_queue);

        //store the fact that the process has started
        pcb->started = 1;

        if (pcb != NULL)
        {
            //process the block
            while (pcb->remaining_burst_time)
            {
                //cycle through
                virtual_cpu(pcb);
                result->total_run_time++;
            }

            //cleanup
            dyn_array_pop_back(ready_queue);
        }

        result->average_wall_clock_time += result->total_run_time;
    }

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
    if (! ready_queue || ! result)
    {
        return false;
    }

    //setup queue
    ProcessControlBlock_t pcb;

    //Prep statistics calculations
    int numProcesses = dyn_array_size(ready_queue);
    result->average_latency_time = 0.0f;
    result->average_wall_clock_time = 0.0f;
    result->total_run_time = 0;

    //run until empty
    while (dyn_array_empty(ready_queue) == false)
    {
        //extract last item in queue
        dyn_array_extract_back(ready_queue, &pcb);

        //set that it has started if haven't done so already
        if (! pcb.started)
        {
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
            dyn_array_push_front(ready_queue, &pcb);
        }
    }

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

}

void* first_come_first_serve_worker (void* input) {

}

void* round_robin_worker (void* input) {

}

