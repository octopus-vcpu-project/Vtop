
/*
 * Copyright (C) 2018-2019 VMware, Inc.
 * SPDX-License-Identifier: GPL-2.0
 */
#include <inttypes.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/sysinfo.h>
#include <limits.h>
#include <assert.h>
#include <iostream>
#include <string.h>
#include <random>
#include <vector>
#define PROBE_MODE	(0)
#define DIRECT_MODE	(1)

#define MAX_CPUS	(192)
#define GROUP_LOCAL	(0)
#define GROUP_NONLOCAL	(1)
#define GROUP_GLOBAL	(2)

#define NR_SAMPLES      (10)
#define SAMPLE_US       (10000)

#define min(a,b)	(a < b ? a : b)
#define LAST_CPU_ID	(min(nr_cpus, MAX_CPUS))
#define PTHREAD_TASK_AMOUNT ((LAST_CPU_ID - (LAST_CPU_ID % 2))/2)


int nr_numa_groups;
int nr_cpus;
int cpu_group_id[MAX_CPUS];
double comm_latency[MAX_CPUS][MAX_CPUS];
int active_cpu_bitmap[MAX_CPUS];
std::vector<int> task_stack;
pthread_t worker_tasks[MAX_CPUS];
static size_t nr_relax = 0;
pthread_mutex_t ready_check = PTHREAD_MUTEX_INITIALIZER;
//static size_t nr_tested_cores = 0;

std::random_device rd;
std::default_random_engine e1(rd());
typedef unsigned atomic_t;


typedef union {
	atomic_t x;
	char pad[1024];
} big_atomic_t __attribute__((aligned(1024)));

typedef struct {
	cpu_set_t cpus;
	atomic_t me;
	atomic_t buddy;
	big_atomic_t *nr_pingpongs;
	atomic_t  *pingpong_mutex;
	int *stoploops;
} thread_args_t;

static inline uint64_t now_nsec(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * ((uint64_t)1000*1000*1000) + ts.tv_nsec;
}

static void common_setup(thread_args_t *args)
{
	if (sched_setaffinity(0, sizeof(cpu_set_t), &args->cpus)) {
		perror("sched_setaffinity");
		exit(1);
	}

	if (args->me == 0) {
		args->pingpong_mutex = (atomic_t*)mmap(0, getpagesize(), PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0);
		if (args->pingpong_mutex == MAP_FAILED) {
			perror("mmap");
			exit(1);
		}
		*(args->pingpong_mutex) = args->me;
	}

	// ensure both threads are ready before we leave -- so that
	// both threads have a copy of pingpong_mutex.
	static pthread_mutex_t wait_mutex = PTHREAD_MUTEX_INITIALIZER;
	static pthread_cond_t wait_cond = PTHREAD_COND_INITIALIZER;
	static int wait_for_buddy = 1;
	pthread_mutex_lock(&wait_mutex);
	if (wait_for_buddy) {
		wait_for_buddy = 0;
		pthread_cond_wait(&wait_cond, &wait_mutex);
	}
	else {
		wait_for_buddy = 1; // for next invocation
		pthread_cond_broadcast(&wait_cond);
	}
	pthread_mutex_unlock(&wait_mutex);
}

static void *thread_fn(void *data)
{
	thread_args_t *args = (thread_args_t *)data;
	common_setup(args);
	big_atomic_t *nr_pingpongs = args->nr_pingpongs;
	atomic_t nr = 0;
	atomic_t me = args->me;
	atomic_t buddy = args->buddy;
	int *stop_loops = args->stoploops;
	atomic_t *cache_pingpong_mutex = args->pingpong_mutex;
	while (1) {
		if (*stop_loops)
			pthread_exit(0);

		if (__sync_bool_compare_and_swap(cache_pingpong_mutex, me, buddy)) {
			++nr;
			if (nr == 10000 && me == 0) {
				__sync_fetch_and_add(&(nr_pingpongs->x), 2 * nr);
				nr = 0;
			}
		}
		for (size_t i = 0; i < nr_relax; ++i)
			asm volatile("rep; nop");
	}
	return NULL;
}

static int measure_latency_pair(int i, int j)
{
	thread_args_t even, odd;
	CPU_ZERO(&even.cpus);
	CPU_SET(i, &even.cpus);
	even.me = 0;
	even.buddy = 1;
	CPU_ZERO(&odd.cpus);
	CPU_SET(j, &odd.cpus);
	odd.me = 1;
	odd.buddy = 0;
    
    int stop_loops = 0;
    atomic_t pingpong_mutex;
	big_atomic_t nr_pingpongs;
	even.nr_pingpongs = &nr_pingpongs;
	odd.nr_pingpongs = &nr_pingpongs;
	even.stoploops = &stop_loops;
	odd.stoploops = &stop_loops;
	return (int)3;
}

static void *thread_fn1(void *data)
{	
	int random_value;
	int random_index;
	while (1) {
		pthread_mutex_lock(&ready_check);
		if(task_stack.size() < 1 ){
			pthread_mutex_unlock(&ready_check);
			break;
		}

		while(1){
        std::uniform_int_distribution<int> uniform_dist(0, task_stack.size() - 1);
		
        random_index = uniform_dist(e1);
        random_value = task_stack[random_index];
		if(active_cpu_bitmap[random_value%LAST_CPU_ID] == 0 && active_cpu_bitmap[(random_value-(random_value%LAST_CPU_ID))/LAST_CPU_ID] == 0 ){
			break;
		}
		std::cout << "ok, random value is:" << random_value<<"planning on using cores"<<random_value%LAST_CPU_ID<<"and"<<(random_value-(random_value%LAST_CPU_ID))/LAST_CPU_ID;
	    }
		active_cpu_bitmap[random_value%LAST_CPU_ID] = 1;
		active_cpu_bitmap[(random_value-(random_value%LAST_CPU_ID))/LAST_CPU_ID] = 1;
		std::swap(task_stack[random_index], task_stack.back());
		task_stack.pop_back();

		pthread_mutex_unlock(&ready_check);
		
		std::cout << "test.\n";
		measure_latency_pair(random_value%LAST_CPU_ID,(random_value-(random_value%LAST_CPU_ID))/LAST_CPU_ID);
		active_cpu_bitmap[random_value%LAST_CPU_ID] = 0;
		active_cpu_bitmap[(random_value-(random_value%LAST_CPU_ID))/LAST_CPU_ID] = 0;
		
		std::cout << "myvector stores " << int(task_stack.size()) << " numbers.\n";
		
		for (int i = 0; i < LAST_CPU_ID; i++) {
			std::cout  << " " << active_cpu_bitmap[i] << " ";
		}
		std::cout << "\n";
	}
	return NULL;
}

static void populate_latency_matrix(void)
{
	int i, j;
	nr_cpus = get_nprocs();
	for (i = 0; i < LAST_CPU_ID; i++) {
		active_cpu_bitmap[i] = 0;
		for (j = i + 1; j < LAST_CPU_ID; j++) {
			task_stack.push_back(LAST_CPU_ID * i + j);
		}
	}
	for (i = 0; i < PTHREAD_TASK_AMOUNT; i++) {
		std::cout << "pthread task amount  " << PTHREAD_TASK_AMOUNT << " numbers.\n";
		thread_args_t newtest;
		pthread_create(&worker_tasks[i], NULL, thread_fn1, &newtest);
	}
	std::cout << "myvector stores " << int(task_stack.size()) << " numbers.\n";
	while(task_stack.size() > 0 ){
		sleep(1);
	}
	for (int i = 0; i < PTHREAD_TASK_AMOUNT; i++) {
    	pthread_join(worker_tasks[i], NULL);
  	}

}

static void print_population_matrix(void)
{
	int i, j;

	for (i = 0; i < LAST_CPU_ID; i++) {
		for (j = 0; j < LAST_CPU_ID; j++)
			printf("%7d", (int)(comm_latency[i][j]));
		printf("\n");
	}
}

static double get_min_latency(int cpu, int group)
{
	int j;
	double min = INT_MAX;

	for (j = 0; j < LAST_CPU_ID; j++) {
		if (comm_latency[cpu][j] == 0)
			continue;

		/* global check */
		if (group == GROUP_GLOBAL && comm_latency[cpu][j] < min)
			min = comm_latency[cpu][j];

		/* local check */
		if (group == GROUP_LOCAL && cpu_group_id[cpu] == cpu_group_id[j]
			&& comm_latency[cpu][j] < min)
			min = comm_latency[cpu][j];

		/* non-local check */
		if (group == GROUP_NONLOCAL && cpu_group_id[cpu] != cpu_group_id[j]
			&& comm_latency[cpu][j] < min)
			min = comm_latency[cpu][j];
	}

	return min == INT_MAX ? 0 : min;
}


static double get_min2_latency(int cpu, int group, double val)
{
	int j;
	double min = INT_MAX;

	for (j = 0; j < LAST_CPU_ID; j++) {
		if (comm_latency[cpu][j] == 0)
			continue;

		/* global check */
		if (group == GROUP_GLOBAL && comm_latency[cpu][j] < min && comm_latency[cpu][j] > val)
			min = comm_latency[cpu][j];

		/* local check */
		if (group == GROUP_LOCAL && cpu_group_id[cpu] == cpu_group_id[j]
			&& comm_latency[cpu][j] < min && comm_latency[cpu][j] > val)
			min = comm_latency[cpu][j];

		/* non-local check */
		if (group == GROUP_NONLOCAL && cpu_group_id[cpu] != cpu_group_id[j]
			&& comm_latency[cpu][j] < min && comm_latency[cpu][j] > val)
			min = comm_latency[cpu][j];
	}

	return min == INT_MAX ? 0 : min;
}

static double get_max_latency(int cpu, int group)
{
	int j;
	double max = -1;

	for (j = 0; j < LAST_CPU_ID; j++) {
		if (comm_latency[cpu][j] == 0)
			continue;

		/* global check */
		if (group == GROUP_GLOBAL && comm_latency[cpu][j] > max)
			max = comm_latency[cpu][j];

		/* local check */
		if (group == GROUP_LOCAL && cpu_group_id[cpu] == cpu_group_id[j]
			&& comm_latency[cpu][j] > max)
			max = comm_latency[cpu][j];

		/* non-local check */
		if (group == GROUP_NONLOCAL && cpu_group_id[cpu] != cpu_group_id[j]
			&& comm_latency[cpu][j] > max)
			max = comm_latency[cpu][j];
	}

	return max == -1 ? INT_MAX : max;
}

/*
 * For proper assignment, the following invariant must hold:
 * The maximum latency between two CPUs in the same group (any group)
 * should be less than the minimum latency between any two CPUs from
 * different groups.
 */
static void validate_group_assignment()
{
	int i;
	double local_max = 0, nonlocal_min = INT_MAX;

	for (i = 0; i < LAST_CPU_ID; i++) {
		local_max = get_max_latency(i, GROUP_LOCAL);
		nonlocal_min = get_min_latency(i, GROUP_NONLOCAL);
		if (local_max == INT_MAX || nonlocal_min == 0)
			continue;

		if(local_max > 1.10 * nonlocal_min) {
			printf("FAIL!!!\n");
			printf("local max is bigger than NonLocal min for CPU: %d %d %d\n",
							i, (int)local_max, (int)nonlocal_min);
			exit(1);
		}
	}
	printf("PASS!!!\n");
}

static void construct_vnuma_groups(void)
{
	int i, j, count, nr_numa_groups = 0;
	double min, min_2;

	/* Invalidate group IDs */
	for (i = 0; i < LAST_CPU_ID; i++)
		cpu_group_id[i] = -1;

	for (i = 0; i < LAST_CPU_ID; i++) {
		/* If already assigned to a vNUMA group, then skip */
		if (cpu_group_id[i] != -1)
			continue;

	 	/* Else, add CPU to the next group and generate a new group id */
		cpu_group_id[i] = nr_numa_groups;
		nr_numa_groups++;

		/* Get min latency */	
		min = get_min_latency(i, GROUP_GLOBAL);
		min_2 = get_min2_latency(i, GROUP_GLOBAL, min);
#if 0
		if (min_2 > 2 * min)
			min = min_2;
#endif
		/* Add all CPUS that are within 40% of min latency to the same group as i */
		for (j = i + 1 ; j < LAST_CPU_ID; j++) {
			//printf("checking %d %d Min: %f pair: %f\n", i, j, min, comm_latency[i][j]);
			if (min >= 100 && comm_latency[i][j] < min * 1.40)
				cpu_group_id[j] = cpu_group_id[i];

			/* allow higher tolerance for small values */
			if (min < 100 && comm_latency[i][j] < min * 1.60)
				cpu_group_id[j] = cpu_group_id[i]; 
		}
	}
#if 0
	for (i = 0; i < LAST_CPU_ID; i++)
		printf("CPUID: %d GroupID: %d\n", i, cpu_group_id[i]);
#endif
	for (i = 0; i < nr_numa_groups; i++) {
		printf("vNUMA-Group-%d", i);
		count = 0;
		for (j = 0; j < LAST_CPU_ID; j++)
			if (cpu_group_id[j] == i) {
				printf("%5d", j);
				count++;
			}
		printf("\t(%d CPUS)\n", count);
	}
}

#define CPU_ID_SHIFT		(16)
/*
 * %4 is specific to our platform.
 */
#define CPU_NUMA_GROUP(mode, i)	(mode == PROBE_MODE ? cpu_group_id[i] : i % 4)
static void configure_os_numa_groups(int mode)
{
	int i;
	unsigned long val;

	/*
	 * pass vcpu & numa group id in a single word using a simple encoding:
	 * first 16 bits store the cpu identifier
	 * next 16 bits store the numa group identifier
	 * */
	for(i = 0; i < LAST_CPU_ID; i++) {
		/* store cpu identifier and left shift */
		val = i;
		val = val << CPU_ID_SHIFT;
		/* store the numa group identifier*/
		val |= CPU_NUMA_GROUP(mode, i);
	}
}


int main(int argc, char **argv)
{
	int c, verbose, mode = PROBE_MODE;
	int nr_pages = 0;

	while ((c = getopt (argc, argv, "dvn:")) != -1) {
		switch (c) {
			case 'd':
				printf("skipping measurements in direct mode...\n");
				mode = DIRECT_MODE;
				break;
			case 'v':
				verbose = 1;
				break;
			case 'n':
				nr_pages = atoi(optarg);
				printf("pages per page-table pool = %d\n", nr_pages);
			default:
				break;
		}
	}
	if (argc == 2 && (!strcmp(argv[1], "--verbose") || !strcmp(argv[1], "-v")))
		verbose = 1;
	uint64_t popul_laten_last = now_nsec();
	printf("populating latency matrix...\n");
	populate_latency_matrix();
	uint64_t popul_laten_now = now_nsec();
	printf("This time it took for latency matrix to be populated%lf\n", (popul_laten_now-popul_laten_last)/(double)1000000);
	if (verbose)
		print_population_matrix();
	printf("constructing NUMA groups...\n");
	popul_laten_last = now_nsec();
	construct_vnuma_groups();
	popul_laten_now = now_nsec();
	printf("This time it took for NUma groups to be contstructed%lf\n", (popul_laten_now-popul_laten_last)/(double)1000000);
	popul_laten_last = now_nsec();
	printf("validating group assignment...");
	validate_group_assignment();
	popul_laten_now = now_nsec();
	printf("This time it took for group assignment to be verified%lf\n", (popul_laten_now-popul_laten_last)/(double)1000000);

	configure_os_numa_groups(mode);
	printf("Done...\n");
}

