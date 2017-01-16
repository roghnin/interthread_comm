//============================================================================
// Name        : interthread_comm.cpp
// Author      : Hs
// Version     :
// Copyright   : Hs
// Description : Hello World in C++, Ansi-style
//============================================================================

#include <iostream>

#include <sys/wait.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>

#include "interthread_comm.h"
using namespace std;

int main(int argc, char **argv) {
	long i, j;

	cpu_set_t cpuset_main;
	pthread_t main_pt;
	int cpu_err_main;

	int c; //for getopt();
	int child_cnt;
	int child_step;
	unsigned long loop_cnt;

	pid_t pid;
	clockid_t clk_id;
	struct timespec tp1, tp2, diff;
	uint64_t cc1, cc2;
	double freq;

	//default values of environment variables
	child_cnt = 1;
	//child_step = 36;
	child_step = 1;
	loop_cnt = 1000000;

	while ((c = getopt (argc, argv, "c:s:l:?")) != -1)
		switch (c)
		  {
		  case 'c':
			child_cnt = atoi(optarg);
			break;
		  case 's':
			child_step = atoi(optarg);
			break;
		  case 'l':
			loop_cnt = atoi(optarg) * 1000000;
			break;
		  case '?':
			if (optopt == 'c' || optopt == 's' || optopt == 'l')
			  fprintf (stderr, "Option -%c requires an argument.\n", optopt);
			else if (isprint (optopt))
			  fprintf (stderr, "Unknown option `-%c'.\n", optopt);
			else
			  fprintf (stderr,
					   "Unknown option character `\\x%x'.\n",
					   optopt);
			return 1;
		  default:
			abort ();
		  }

	printf("loop_cnt:%lu\n",loop_cnt);
	printf("child_cnt:%d\n",child_cnt);
	printf("child_step:%d\n",child_step);


	pid = getpid();
	clock_getcpuclockid(pid, &clk_id);

	//pin the Main thread to core0
	CPU_ZERO(&cpuset_main);
	CPU_SET(0,&cpuset_main);
	main_pt=pthread_self();
	cpu_err_main = pthread_setaffinity_np(main_pt, sizeof(cpu_set_t), &cpuset_main);
	printf("cpu_e0=%d\n",cpu_err_main);
	for (j=0;j<CORE_CNT;j++){
		if (CPU_ISSET(j, &cpuset_main)){
			printf("main assigned to core %lu\n",j);
		}
	}

	flip_flop[child_cnt]=1;//main initialization.

	/*
	 * creating children.
	 *
	 * It works like this:
	 * child[i] is pinned to core child_step*(i+1).
	 * Main thread is pinned to core 0.
	 * child[i] waits flip_flop[i] to turn to 0, then set it back to 1,
	 * and set flip_flop[i+1] to 0.
	 *
	 * Main thread waits flip_flop[child_cnt] to turn to 0, then
	 * set it back to 1, and set flip_flop[0] to 0.
	 */
	child_list list(child_step, child_cnt);

	printf("hello from main.\n");

	//start clock after the first round to avoid waiting.

	while(flip_flop[child_cnt]==1);
	flip_flop[child_cnt]=1;
	flip_flop[0]=0;
	for(j=0;j<5;j++){
		/*if (j==2){
			//pthread_create(&lazy_pt, NULL, lazy_child, 36);
			pthread_create(&busy_pt, NULL, busy_child, NULL);
		}*/
		clock_gettime(clk_id, &tp1);
		cc1 = rdtsc();
		for (i=0;i<loop_cnt;i++){
			while(flip_flop[child_cnt]==1);
			flip_flop[child_cnt]=1;
			flip_flop[0]=0;
			//pthread_yield();
		}
		cc2 = rdtsc();
		clock_gettime(clk_id, &tp2);
		timespec_diff(&tp1,&tp2,&diff);
		//printf("interval: %lu.%lu\n",diff.tv_sec,diff.tv_nsec);
		//timespec_to_ns(&diff);
		printf("roundtrip(ns): %lu\n",ns_in_each_loop(&diff, loop_cnt));
		printf("cycle count:%lu\n",cc2-cc1);
		printf("cycles per round:%lu\n",(cc2-cc1)/loop_cnt);
		freq = (double)((cc2-cc1)/loop_cnt) / (double)ns_in_each_loop(&diff, loop_cnt);
		printf("approx. frequency(GHz):%f\n",freq);

	}
	return 0;
}
