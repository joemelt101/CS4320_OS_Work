#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <dyn_array.h>
#include "../include/processing_scheduling.h"


// private function
void virtual_cpu(ProcessControlBlock_t* process_control_block) {
	// decrement the burst time of the pcb
	--process_control_block->remaining_burst_time;
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
    
    while (dyn_array_empty(ready_queue) == false)
    {
        result->average_latency_time += result->total_run_time;
        
        //crank through another process
        pcb = (ProcessControlBlock_t *)dyn_array_back(ready_queue);
        
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


