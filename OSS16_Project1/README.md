# Project 1 -  Multithreaded Process Scheduling

##Submission Tags:

1. Project 1 Milestone 1: `P1M1`
1. Project 1 Milestone 2: `P1M2`
1. Project 1 Final: `P1M3`

Times:

1 FCFS Core: 3:47.12 total
1 RR Core: 3:47.12 total
1 FCFS and 1 RR Core: 1:56.07 total
2 FCFS and 2 RR Cores: 58.03 total

Output:


---------Basic Tests:

time ./process_analysis PCBs.bin FCFS
./process_analysis PCBs.bin FCFS  0.00s user 0.02s system 0% cpu 3:47.12 total

time ./process_analysis PCBs.bin RR
./process_analysis PCBs.bin RR  0.02s user 0.00s system 0% cpu 3:47.14 total

time ./process_analysis PCBs.bin FCFS RR
./process_analysis PCBs.bin FCFS RR  0.00s user 0.02s system 0% cpu 1:56.07 total

time ./process_analysis PCBs.bin FCFS FCFS RR RR
./process_analysis PCBs.bin FCFS FCFS RR RR  0.00s user 0.01s system 0% cpu 57.039 total

---------Extra Credit:

time ./process_analysis PCBs.bin FCFS FCFS FCFS RR RR
./process_analysis PCBs.bin FCFS FCFS FCFS RR RR  0.00s user 0.01s system 0% cpu 48.045 total

time ./process_analysis PCBs.bin FCFS FCFS RR RR RR RR
./process_analysis PCBs.bin FCFS FCFS RR RR RR RR  0.00s user 0.02s system 0% cpu 41.032 total
