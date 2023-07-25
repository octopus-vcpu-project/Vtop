
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
#include <fstream>
#include <sstream>
#include <sys/syscall.h>
#include <unordered_map>
#define PROBE_MODE	(0)
#define DIRECT_MODE	(1)

#define MAX_CPUS	(192)
#define GROUP_LOCAL	(0)
#define GROUP_NONLOCAL	(1)
#define GROUP_GLOBAL	(2)


#define min(a,b)	(a < b ? a : b)
#define LAST_CPU_ID	(min(nr_cpus, MAX_CPUS))

int nr_cpus;
int PTHREAD_TASK_AMOUNT=10;
int verbose = 0;
int NR_SAMPLES = 30;
int SAMPLE_US = 10000;
int cpu_group_id[MAX_CPUS];
int nr_numa_groups;
int nr_pair_groups;
int cpu_pair_id[MAX_CPUS];
int cpu_tt_id[MAX_CPUS];
int ready_counter = 0;
int active_cpu_bitmap[MAX_CPUS];
int finished = 0;
int return_pair = 0;
std::vector<int> task_stack;
std::vector<std::vector<int>> top_stack;
pthread_t worker_tasks[MAX_CPUS];
static size_t nr_relax = 0;
pthread_mutex_t ready_check = PTHREAD_MUTEX_INITIALIZER;
//static size_t nr_tested_cores = 0;
pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
std::random_device rd;
std::default_random_engine e1(rd());
typedef unsigned atomic_t;

typedef std::vector<int> ind_thread;
typedef std::vector<ind_thread> core_pair;
typedef std::vector<core_pair> numa_gr;

std::vector<std::vector<int>> numa_to_pair_arr;
std::vector<std::vector<int>> pair_to_thread_arr;
std::vector<std::vector<int>> thread_to_cpu_arr;
std::vector<int> numas_to_cpu;
std::vector<int> pairs_to_cpu;
std::vector<int> threads_to_cpu;
std::vector<numa_gr> numa_array;


int get_representative(const ind_thread& it) {
    // Check for empty vector
    if (it.empty()) {
        throw std::runtime_error("ind_thread is empty");
    }
    return it[0];
}

int get_representative(const core_pair& cp) {
    if (cp.empty()) {
        throw std::runtime_error("core_pair is empty");
    }
    return get_representative(cp[0]); 
}

int get_representative(const numa_gr& ng) {
    if (ng.empty()) {
        throw std::runtime_error("numa_gr is empty");
    }
    return get_representative(ng[0]); 
}


void add_int_to_array(int n, ind_thread& arr) {
    arr.push_back(n);
}

void add_int_to_array(int n, core_pair& arr) {
    ind_thread it;
    it.push_back(n);
    arr.push_back(it);
}

void add_int_to_array(int n, numa_gr& arr) {
    ind_thread it;
    it.push_back(n);
    core_pair cp;
    cp.push_back(it);
    arr.push_back(cp);
}



void moveThreadtoHighPrio(pid_t tid) {

    std::string path = "/sys/fs/cgroup/hi_prgroup/cgroup.threads";
    std::ofstream ofs(path, std::ios_base::app);
    if (!ofs) {
        std::cerr << "Could not open the file\n";
        return;
    }
    ofs << tid << "\n";
    ofs.close();
}

void moveCurrentThread() {
    pid_t tid;
    tid = syscall(SYS_gettid);
    std::string path = "/sys/fs/cgroup//hi_prgroup/cgroup.procs";
    std::ofstream ofs(path, std::ios_base::app);
    if (!ofs) {
        std::cerr << "Could not open the file\n";
        return;
    }
    ofs << tid << "\n";
    ofs.close();
    struct sched_param params;
    params.sched_priority = sched_get_priority_max(SCHED_RR);
    sched_setscheduler(tid,SCHED_RR,&params);
}

std::string_view get_option(
    const std::vector<std::string_view>& args, 
    const std::string_view& option_name) {
    for (auto it = args.begin(), end = args.end(); it != end; ++it) {
        if (*it == option_name)
            if (it + 1 != end)
                return *(it + 1);
    }
    
    return "";
};


bool has_option(
    const std::vector<std::string_view>& args, 
    const std::string_view& option_name) {
    for (auto it = args.begin(), end = args.end(); it != end; ++it) {
        if (*it == option_name)
            return true;
    }
    
    return false;
};


void setArguments(const std::vector<std::string_view>& arguments) {
    verbose = has_option(arguments, "-v");
    
    auto set_option_value = [&](const std::string_view& option, int& target) {
        if (auto value = get_option(arguments, option); !value.empty()) {
            try {
                target = std::stoi(std::string(value));
            } catch(const std::invalid_argument&) {
                throw std::invalid_argument(std::string("Invalid argument for option ") + std::string(option));
            } catch(const std::out_of_range&) {
                throw std::out_of_range(std::string("Out of range argument for option ") + std::string(option));
            }
        }
    };
    
    set_option_value("-p", PTHREAD_TASK_AMOUNT);
    set_option_value("-s", NR_SAMPLES);
    set_option_value("-u", SAMPLE_US);
}


typedef union {
	atomic_t x;
	char pad[1024];
} big_atomic_t __attribute__((aligned(1024)));

typedef struct {
	cpu_set_t cpus;
	atomic_t me;
	atomic_t buddy;
	big_atomic_t *nr_pingpongs;
	atomic_t  **pingpong_mutex;
	int *stoploops;
	pthread_mutex_t* mutex;
    pthread_cond_t* cond;
    int* flag;
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
		*(args->pingpong_mutex) = (atomic_t*)mmap(0, getpagesize(), PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0);
		if (*(args->pingpong_mutex) == MAP_FAILED) {
			perror("mmap");
			exit(1);
		}
		*(*(args->pingpong_mutex)) = args->me;
	}

	// ensure both threads are ready before we leave -- so that
	// both threads have a copy of pingpong_mutex.
	pthread_mutex_lock(args->mutex);
    if (*(args->flag)) {
        *(args->flag) = 0;
        pthread_cond_wait(args->cond, args->mutex);
    } else {
        *(args->flag) = 1;
        pthread_cond_broadcast(args->cond);
    }
    pthread_mutex_unlock(args->mutex);
	
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
	atomic_t *cache_pingpong_mutex = *(args->pingpong_mutex);
	while (1) {
		
		if (*stop_loops == 1){
			pthread_exit(0);
		}
		if (__sync_bool_compare_and_swap(cache_pingpong_mutex, me, buddy)) {
			++nr;
			if (nr == 1000 && me == 0) {
				__sync_fetch_and_add(&(nr_pingpongs->x), 2 * nr);
				nr = 0;
			}
		}
		for (size_t i = 0; i < nr_relax; ++i)
			asm volatile("rep; nop");
		
	}
	return NULL;
}

int measure_latency_pair(int i, int j)
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
    atomic_t* pingpong_mutex = (atomic_t*) malloc(sizeof(atomic_t));;
	big_atomic_t nr_pingpongs;
	even.nr_pingpongs = &nr_pingpongs;
	odd.nr_pingpongs = &nr_pingpongs;
	even.stoploops = &stop_loops;
	odd.stoploops = &stop_loops;
	even.pingpong_mutex = &pingpong_mutex;
	odd.pingpong_mutex = &pingpong_mutex;
	pthread_mutex_t wait_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t wait_cond = PTHREAD_COND_INITIALIZER;
    int wait_for_buddy = 1;

    even.mutex = &wait_mutex;
    odd.mutex = &wait_mutex;
    even.cond = &wait_cond;
    odd.cond = &wait_cond;
    even.flag = &wait_for_buddy;
    odd.flag = &wait_for_buddy;

	__sync_lock_test_and_set(&nr_pingpongs.x, 0);

	pthread_t t_odd, t_even;
	if (pthread_create(&t_odd, NULL, thread_fn, &odd)) {
		printf("ERROR creating odd thread\n");
		exit(1);
	}
	if (pthread_create(&t_even, NULL, thread_fn, &even)) {
		printf("ERROR creating even thread\n");
		exit(1);
	}

	uint64_t last_stamp = now_nsec();
	double best_sample = 1./0.;
	int test = 0;
	for (test = 0; test < NR_SAMPLES; test++) {
		usleep(SAMPLE_US);
		atomic_t s = __sync_lock_test_and_set(&nr_pingpongs.x, 0);
		uint64_t time_stamp = now_nsec();
		double sample = (time_stamp - last_stamp) / (double)s;
		last_stamp = time_stamp;


		if ((sample < best_sample && sample != 1.0/0.)||(best_sample==1.0/0.)){
			//if((!best_sample==1.0/0.)&&((best_sample-sample)/best_sample > 0.05)){
                         //       test = test - 10;
                        //}
			best_sample = sample;
		}

	}
	stop_loops = 1;
	pthread_join(t_odd, NULL);
	pthread_join(t_even, NULL);
	stop_loops = 0;
	odd.buddy = 0;
	pingpong_mutex = NULL;
	std::cout << "I:"<<i<<" J:"<<j<<" Sample passed " << (int)(best_sample*100) << " next.\n";
	return (int)(best_sample*100);
}

int stick_this_thread_to_core(int core_id) {
   int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
   if (core_id < 0 || core_id >= num_cores)
      return EINVAL;

   cpu_set_t cpuset;
   CPU_ZERO(&cpuset);
   CPU_SET(core_id, &cpuset);

   pthread_t current_thread = pthread_self();    
   return pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
}




int get_pair_to_test(){

	bool valid_pair_exists = false;
	int last_pair = -1;

	for(int i=0;i<LAST_CPU_ID;i++){
		if(active_cpu_bitmap[i]==1){
			continue;
		}
		for(int j=0;j<LAST_CPU_ID;j++){
			if(active_cpu_bitmap[j]==1){
				continue;
			}
			if(top_stack[i][j] == 0){
					top_stack[i][j] == -1;
					return(i * LAST_CPU_ID + j);
			}

		}
	}
	
	//We're testing 
	if(valid_pair_exists){
		return -1;
	}

	return -2;
}

int get_latency_class(int latency){
	if(latency<0){
		return 1;
	}

	if(latency< 1000){
		return 2;
	}
	if(latency< 8000){
		return 3;
	}
	
	return 4;
}

void set_latency_pair(int x,int y,int latency_class){
	top_stack[x][y] = latency_class;
	top_stack[y][x] = latency_class;
}

void apply_optimization(void){
	int sub_rel;
	for(int x=0;x<LAST_CPU_ID;x++){
		for(int y=0;y<LAST_CPU_ID;y++){
			sub_rel = top_stack[y][x];
			for(int z=0;z<LAST_CPU_ID;z++){
				if((top_stack[y][z]<sub_rel && top_stack[y][z]!=0) && top_stack[x][z] == 0){
					set_latency_pair(x,z,sub_rel);
				}
			}
		}
	}
}


void apply_optimization_recur(int cpu, int last_cpu,int latency_class,std::unordered_map<int,int>& tested_arr){
	tested_arr[cpu] = 1;
	int sub_rel = top_stack[cpu][last_cpu];
	for(int x=0;x<LAST_CPU_ID;x++){
		if(top_stack[last_cpu][x]!=0 && (top_stack[last_cpu][x] < sub_rel && top_stack[cpu][x]==0)){
			set_latency_pair(cpu,x,sub_rel);
		}
	}
	for(int x=0;x<LAST_CPU_ID;x++){
		if((top_stack[cpu][x] != 0 && tested_arr[x] != 1)){
			apply_optimization_recur(x,cpu,latency_class,tested_arr);
		}

	}

}





static void print_population_matrix(void)
{
	int i, j;

	for (i = 0; i < LAST_CPU_ID; i++) {
		for (j = 0; j < LAST_CPU_ID; j++)
			printf("%7d", (int)(top_stack[i][j]));
		printf("\n");
	}
}

static double get_min_latency(int cpu, int group)
{
	int j;
	double min = INT_MAX;

	for (j = 0; j < LAST_CPU_ID; j++) {
		if (top_stack[cpu][j] == 0)
			continue;

		/* global check */
		if (group == GROUP_GLOBAL && top_stack[cpu][j] < min)
			min = top_stack[cpu][j];

		/* local check */
		if (group == GROUP_LOCAL && cpu_group_id[cpu] == cpu_group_id[j]
			&& top_stack[cpu][j] < min)
			min = top_stack[cpu][j];

		/* non-local check */
		if (group == GROUP_NONLOCAL && cpu_group_id[cpu] != cpu_group_id[j]
			&& top_stack[cpu][j] < min)
			min = top_stack[cpu][j];
	}

	return min == INT_MAX ? 0 : min;
}


static double get_min2_latency(int cpu, int group, double val)
{
	int j;
	double min = INT_MAX;

	for (j = 0; j < LAST_CPU_ID; j++) {
		if (top_stack[cpu][j] == 0)
			continue;

		/* global check */
		if (group == GROUP_GLOBAL && top_stack[cpu][j] < min && top_stack[cpu][j] > val)
			min = top_stack[cpu][j];

		/* local check */
		if (group == GROUP_LOCAL && cpu_group_id[cpu] == cpu_group_id[j]
			&& top_stack[cpu][j] < min && top_stack[cpu][j] > val)
			min = top_stack[cpu][j];

		/* non-local check */
		if (group == GROUP_NONLOCAL && cpu_group_id[cpu] != cpu_group_id[j]
			&& top_stack[cpu][j] < min && top_stack[cpu][j] > val)
			min = top_stack[cpu][j];
	}

	return min == INT_MAX ? 0 : min;
}

static double get_max_latency(int cpu, int group)
{
	int j;
	double max = -1;

	for (j = 0; j < LAST_CPU_ID; j++) {
		if (top_stack[cpu][j] == 0)
			continue;

		/* global check */
		if (group == GROUP_GLOBAL && top_stack[cpu][j] > max)
			max = top_stack[cpu][j];

		/* local check */
		if (group == GROUP_LOCAL && cpu_group_id[cpu] == cpu_group_id[j]
			&& top_stack[cpu][j] > max)
			max = top_stack[cpu][j];

		/* non-local check */
		if (group == GROUP_NONLOCAL && cpu_group_id[cpu] != cpu_group_id[j]
			&& top_stack[cpu][j] > max)
			max = top_stack[cpu][j];
	}

	return max == -1 ? INT_MAX : max;
}

int find_numa_groups(void)
{
	nr_numa_groups = 0;
	bool finished=false;
	for(int i = 0;i<LAST_CPU_ID;i++){
		cpu_group_id[i] = -1;
	}
	for (int i = 0; i < LAST_CPU_ID; i++) {
		if(cpu_group_id[i] != -1){
			continue;
		}
		cpu_group_id[i] == nr_numa_groups;
		for (int j = 0; j < LAST_CPU_ID; j++) {
			if(cpu_group_id[j] != -1){
				continue;
			}
			if(top_stack[i][j] == 0 ){
				if(i==0 && j==1){
					NR_SAMPLES = NR_SAMPLES*20;
				}
				int latency = measure_latency_pair(i,j);
				set_latency_pair(i,j,get_latency_class(latency));
				if(i==0 && j==1){
					NR_SAMPLES = NR_SAMPLES/20;
				}
			}
			if(top_stack[i][j] < 4){
				cpu_group_id[j] = nr_numa_groups;
			}
		}
		nr_numa_groups++;
	}
	apply_optimization();
	return nr_numa_groups;
}

typedef struct {
	std::vector<int> pairs_to_test;
} worker_thread_args;

//TODO convert to something more parallel
void ST_find_topology(std::vector<int> input){
	for(int x=0;x<input.size();x++){
		int j = input[x] % LAST_CPU_ID;
		int i = (input[x] - j)/LAST_CPU_ID;
		
		std::cout<<"here"<<i<<"here"<<j<<std::endl;
		if(top_stack[i][j] == 0){
			int latency = measure_latency_pair(i,j);
			set_latency_pair(i,j,get_latency_class(latency));
			apply_optimization();
		}
	}
}

static void *thread_fn2(void *data)
{
	worker_thread_args *args = (worker_thread_args *)data;
	ST_find_topology(args->pairs_to_test);

	pthread_mutex_lock(&ready_check);
 	ready_counter += 1;
	pthread_mutex_unlock(&ready_check);
	pthread_cond_signal(&cv);

	return NULL;
}


void MT_find_topology(void){
	pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
	std::vector<std::vector<int>> all_pairs_to_test(nr_numa_groups);
	int amount = 0;
	for(int i=0;i<LAST_CPU_ID;i++){
		for(int j=i+1;j<LAST_CPU_ID;j++){
			if(top_stack[i][j] == 0){
				all_pairs_to_test[amount % nr_numa_groups].push_back(i * LAST_CPU_ID + j);
				amount++;
			}
		}
	}
	pthread_t worker_tasks[nr_numa_groups];
	for (int i = 0; i < nr_numa_groups; i++) {
		worker_thread_args wrk_args;
		wrk_args.pairs_to_test = all_pairs_to_test[i];
		pthread_create(&worker_tasks[i], NULL, thread_fn2, &wrk_args);
	}
	std::cout<<"here"<<std::endl;

	pthread_mutex_lock(&ready_check);
	while(ready_counter != nr_numa_groups){
		pthread_cond_wait(&cv, &ready_check);
	}
	pthread_mutex_unlock(&ready_check);
	
	std::cout<<"here"<<std::endl;
	for (int i = 0; i < nr_numa_groups; i++) {
    		pthread_join(worker_tasks[i], NULL);
  	}
	ready_counter = 0;
}




bool verify_numa_group(std::vector<int> input){
	std::vector<int> nums;
	for (int i = 0; i < input.size(); ++i) {
        	if (input[i] == 1) {
            		nums.push_back(i);
        	}
    	}
	for(int i=0; i < nums.size();i++){
		for(int j=i+1;j<nums.size();j++){
			int latency = measure_latency_pair(pairs_to_cpu[nums[i]],pairs_to_cpu[nums[j]]);
			if(get_latency_class(latency) != 3){
				return false;
			}
		}
	}
	return true;
}

bool verify_thread_group(std::vector<int> input){
        std::vector<int> nums;
        for (int i = 0; i < input.size(); ++i) {
                if (input[i] == 1) {
                        nums.push_back(i);
                }
        }
        for(int i=0; i < nums.size();i++){
                for(int j=i+1;j<nums.size();j++){
                        int latency = measure_latency_pair(nums[i],nums[j]);
                        //TODO save
			if(get_latency_class(latency) != 1){
				return false;
                        }
                }
        }
        return true;
}


bool verify_pair_group(std::vector<int> input){
        std::vector<int> nums;
        for (int i = 0; i < input.size(); ++i) {
                if (input[i] == 1) {
                        nums.push_back(i);
                }
        }
        for(int i=0; i < nums.size();i++){
                for(int j=i+1;j<nums.size();j++){
                        int latency = measure_latency_pair(threads_to_cpu[nums[i]],threads_to_cpu[nums[j]]);
                        //TODO save
                        if(get_latency_class(latency) != 2){
                                return false;
                        }
                }
        }
        return true;
}



bool verify_topology(void){
	
	for(int i = 0; i< thread_to_cpu_arr.size();i++) {
		if(!verify_thread_group(thread_to_cpu_arr[i])){
			return false;
		}
	}

	for (int i = 0; i < pair_to_thread_arr.size(); i+=1) {
                if(!verify_pair_group(pair_to_thread_arr[i])){
                        return false;
                }
        }

	for (int i = 0; i < numa_to_pair_arr.size(); i+=1) {
		if(!verify_numa_group(numa_to_pair_arr[i])){
			return false;
		}
	}
	NR_SAMPLES = NR_SAMPLES*2;
        SAMPLE_US = SAMPLE_US*2;



	for(int i=0;i<nr_numa_groups;i++){
                for(int j=i+1;j<nr_numa_groups;j++){
			int latency = measure_latency_pair(numas_to_cpu[i],numas_to_cpu[i+1]);
                	if(get_latency_class(latency) != 4){
                        	return false;
                	}
		}
        }

	return true;
}

static void construct_vnuma_groups(void)
{
	int i, j, count = 0;
	nr_numa_groups = 0;
	nr_pair_groups = 0;
	int nr_tt_groups = 0;
	double min, min_2;
	nr_cpus = get_nprocs();
	for (i = 0; i < LAST_CPU_ID; i++){
		cpu_group_id[i] = -1;
		cpu_pair_id[i] = -1;
		cpu_tt_id[i] = -1;
	}


	for (i = 0; i < LAST_CPU_ID; i++) {
		
		if (cpu_group_id[i] == -1){
			cpu_group_id[i] = nr_numa_groups;
			nr_numa_groups++;
			std::vector<int> new_thing(LAST_CPU_ID);
			numa_to_pair_arr.push_back(new_thing);
			numas_to_cpu.push_back(i);
		}
		if (cpu_pair_id[i] == -1){
			cpu_pair_id[i] = nr_pair_groups;
			nr_pair_groups++;
			std::vector<int> newer_thing(LAST_CPU_ID);
			pair_to_thread_arr.push_back(newer_thing);
			pairs_to_cpu.push_back(i);
		}
		if (cpu_tt_id[i] == -1){
			cpu_tt_id[i] = nr_tt_groups;
			nr_tt_groups++;
			threads_to_cpu.push_back(i);
			std::vector<int> cpu_bitmap(LAST_CPU_ID);
                        thread_to_cpu_arr.push_back(cpu_bitmap);

		}

		
		for (j = 0 ; j < LAST_CPU_ID; j++) {
				if (top_stack[i][j]<4 && cpu_group_id[i] != -1){
					cpu_group_id[j] = cpu_group_id[i];
				}
				if (top_stack[i][j]<3 && cpu_pair_id[i] != -1){
					cpu_pair_id[j] = cpu_pair_id[i];
				}
				if (top_stack[i][j]<2 && cpu_tt_id[i] != -1){
					cpu_tt_id[j] = cpu_tt_id[i];
				}

		}
		numa_to_pair_arr[cpu_group_id[i]][cpu_pair_id[i]] = 1;
		pair_to_thread_arr[cpu_pair_id[i]][cpu_tt_id[i]] = 1;
		thread_to_cpu_arr[cpu_tt_id[i]][i] = 1;
	}

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
	printf("%d ", nr_numa_groups);	
	printf("%d ", nr_pair_groups);	
	printf("%d ", nr_tt_groups);	
	printf("\n");


	
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


int main(int argc, char *argv[])
{
	moveCurrentThread();
	moveThreadtoHighPrio(syscall(SYS_gettid));
	nr_cpus = get_nprocs();
	for (int i = 0; i < LAST_CPU_ID; i++) {
		std::vector<int> cpumap(LAST_CPU_ID);
		top_stack.push_back(cpumap);
	}
	for(int p=0;p< LAST_CPU_ID;p++){
		top_stack[p][p] = 1;
	}
	const std::vector<std::string_view> args(argv, argv + argc);
  	setArguments(args);
	uint64_t popul_laten_last = now_nsec();
	printf("Finding NUMA groups...\n");
	int numa_groups = find_numa_groups();

	NR_SAMPLES = NR_SAMPLES /2;
	SAMPLE_US = SAMPLE_US/2;

	std::cout<<"numa group"<<numa_groups<<std::endl;
	uint64_t popul_laten_now = now_nsec();
	printf("This time it took to find NUMA Groups%lf\n", (popul_laten_now-popul_laten_last)/(double)1000000);
	popul_laten_last = now_nsec();
	printf("Consturcting overall topology...\n");
	MT_find_topology();
	popul_laten_now = now_nsec();
	printf("This time it took to find all topology%lf\n", (popul_laten_now-popul_laten_last)/(double)1000000);
	
	construct_vnuma_groups();
	if(verbose){
		print_population_matrix();
	}
	popul_laten_last = now_nsec();

	if (verify_topology()){
		printf("TOPOLOGY IS VERIFIED...\n");
	}else{
		printf("FAILED VERIFICATION \n");
	}
	popul_laten_now = now_nsec();
	printf("This time it took to verify%lf\n", (popul_laten_now-popul_laten_last)/(double)1000000);

	configure_os_numa_groups(1);
	printf("Done...\n");
}

