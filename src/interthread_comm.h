/*
 * interthread_comm.h
 *
 *  Created on: Jan 9, 2017
 *      Author: hwen5
 */

#ifndef INTERTHREAD_COMM_H_
#define INTERTHREAD_COMM_H_

#define CHILD_CNT_MAX 12
#define CORE_CNT 72
#define NS_IN_SEC 1000000000

#include <atomic>
#include <cassert>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
//#include <papi.h>

//volatile int flip_flop[CHILD_CNT_MAX];// changed it into atom tags.
std::atomic<int> flip_flop[CHILD_CNT_MAX];

typedef struct child_args{
	int id;
	int core;
}child_args;

uint64_t rdtsc(){
    unsigned int lo,hi;
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    return ((uint64_t)hi << 32) | lo;
}

void timespec_diff(struct timespec *start, struct timespec *stop, struct timespec *result){
	if ((stop->tv_nsec-start->tv_nsec)<0) {
		result->tv_sec=stop->tv_sec-start->tv_sec-1;
		result->tv_nsec=stop->tv_nsec-start->tv_nsec+NS_IN_SEC;
	} else {
		result->tv_sec= stop->tv_sec-start->tv_sec;
		result->tv_nsec= stop->tv_nsec-start->tv_nsec;
	}
}

unsigned long ns_in_each_loop(struct timespec *total, unsigned long cnt){
	unsigned long long ns = total->tv_sec*NS_IN_SEC + total->tv_nsec;
	return (unsigned long)(ns / cnt);
	/* original code playing with loop_cnt and decimal bits. */
	/*
	 * ts->tv_sec *= (1000000000/LOOP_CNT);
	 * ts->tv_sec += ts->tv_nsec/LOOP_CNT;
	 * ts->tv_nsec = (ts->tv_nsec%LOOP_CNT)*(1000000000/LOOP_CNT);
	 */
}

void* child(void *Args){
	struct child_args *args = (child_args *)Args;
	int j;
	int next_flip_flop;
	//pthread_t child_pt;
	cpu_set_t cpuset;
	int cpu_err;

	//printf("[child]args->id:%d, args->core:%d\n",args->id, args->core);

	//pinning the thread onto certain core.
	//it's moved to the constructor of class child_thread tentatively.
	/*
	CPU_ZERO(&cpuset);
	CPU_SET(args->core, &cpuset);
	//the pthread_self() here might cause some unexpected problem (or optimizations).
	printf("pthread_self():%lu\n",pthread_self());

	cpu_err = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
	for (j=0;j<CORE_CNT;j++){
		if (CPU_ISSET(j, &cpuset)){
			printf("child%d assigned to core %d\n",args->id,j);
		}
	}
	 */


	next_flip_flop = args->id+1;
	while(1){
		while(flip_flop[args->id]==1);
		//flip_flop[args->id]=1;
		flip_flop[args->id].store(1);
		//flip_flop[next_flip_flop]=0;
		flip_flop[next_flip_flop].store(0);
		//pthread_yield();
	}
	pthread_exit(NULL);
	//return NULL;
}

class child_thread{
public:
	int id;
	int core;
	//cpu_set_t *cpu_set;
	pthread_t pt;

	child_thread(int ID, int Core):
		id(ID), core(Core)
	{
		child_args *args;

		args = new child_args;
		args->id = id;
		args->core = core;

		//tentatively, pin the current thread onto a new core, create a child thread, and pin
		//it back.
		int j;
		int next_flip_flop;
		//pthread_t child_pt;
		cpu_set_t cpuset;
		int cpu_err;
		CPU_ZERO(&cpuset);
		CPU_SET(args->core, &cpuset);
		cpu_err = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
		for (j=0;j<CORE_CNT;j++){
			if (CPU_ISSET(j, &cpuset)){
				printf("parent assigned to core %d for child %d\n",j,args->id);
			}
		}

		printf("pthread_create:%d\n",pthread_create(&pt, NULL, &child, (void *)args));

		CPU_ZERO(&cpuset);
		CPU_SET(0, &cpuset);
		cpu_err = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
		for (j=0;j<CORE_CNT;j++){
			if (CPU_ISSET(j, &cpuset)){
				printf("parent assigned back to core %d\n",j);
			}
		}

		//printf("child_pt:%lu\n",pt);

	}
};

class child_list{
public:
	//int child_cnt_max;//might not be useful.
	int child_step;
	int child_cnt;
	child_thread** list;

	child_list(int step, int cnt):
		child_step(step), child_cnt(cnt)
	{
		list = new child_thread*[child_cnt];
		for (int i=0;i<child_cnt;i++){
			list[i] = new child_thread(i,(i+1)*step);
			sleep(1);
		}
	}
};


#endif /* INTERTHREAD_COMM_H_ */
