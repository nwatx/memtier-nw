#include <asm/unistd.h>
#include <assert.h>
#include <linux/perf_event.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <list>
#include <map>
#include <vector>

#include "pebs.h"

using namespace std;

static struct perf_event_mmap_page *perf_page[PEBS_NPROCS][NPBUFTYPES];
int pfd[PEBS_NPROCS][NPBUFTYPES];

int zero_sample_epoch[PEBS_NPROCS] = {0};

// Stats related
__u64 thread_samples[PEBS_NPROCS][NPBUFTYPES] = {0};
map<__u64, __u64> page_counts[PEBS_NPROCS];
#ifdef PEBS_PERIODIC
map<__u64, vector<__u64>> total_page_counts;
#endif
unsigned int  epoch_cnt      = 0;
unsigned long throttle_cnt   = 0;
unsigned long unthrottle_cnt = 0;

// Input args
unsigned long sample_period = 0;
unsigned int  stats_period  = 5;
FILE *output_fp;

int *scan_thread_args;

static long perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
    int cpu, int group_fd, unsigned long flags)
{
  int ret;

  ret = syscall(__NR_perf_event_open, hw_event, pid, cpu,
		group_fd, flags);
  return ret;
}

static struct perf_event_mmap_page* perf_setup(__u64 config, __u64 config1, __u64 cpu, __u64 type)
{
  struct perf_event_attr attr;

  memset(&attr, 0, sizeof(struct perf_event_attr));

  attr.type = PERF_TYPE_RAW;
  attr.size = sizeof(struct perf_event_attr);

  attr.config = config;
  attr.config1 = config1;
  if (sample_period == 0) {
      attr.sample_period = SAMPLE_PERIOD;
  } else {
    attr.sample_period = sample_period;
  }

  attr.sample_type = PERF_SAMPLE_IP | PERF_SAMPLE_TID | PERF_SAMPLE_ADDR;
  attr.pinned = 1;
  attr.disabled = 0;
  //attr.inherit = 1;
  attr.exclude_kernel = 1;
  attr.exclude_hv = 1;
  attr.exclude_callchain_kernel = 1;
  attr.exclude_callchain_user = 1;
  attr.precise_ip = 1;

  pfd[cpu][type] = perf_event_open(&attr, -1, (cpu + START_CPU), -1, 0);
  if(pfd[cpu][type] == -1) {
    printf("perf_event_open\n");
  }
  assert(pfd[cpu][type] != -1);

  size_t mmap_size = sysconf(_SC_PAGESIZE) * PERF_PAGES;
  struct perf_event_mmap_page *p = (struct perf_event_mmap_page *)mmap(NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, pfd[cpu][type], 0);
  if(p == MAP_FAILED) {
    printf("mmap failed\n");
  }
  assert(p != MAP_FAILED);

  return p;
}

#define ZERO_SAMPLE_THRESHOLD (1 * 1000 * 1000UL) // in us

void restart_stuck_threads(void)
{
  // Need to check if any thread is stuck
  // A thread is considered stuck if:
  // 1) It has not generated any samples in the last 1 second AND
  // 2) Majority of the other threads have generated samples during the same period

  int zero_sample_epochs_threshold = ZERO_SAMPLE_THRESHOLD / stats_period;

  bool possibly_stuck[PEBS_NPROCS];

  for (int i = 0; i < PEBS_NPROCS; i++) {
    possibly_stuck[i] = false;
    int samples_current_epoch = 0;
    for (int j = 0; j < NPBUFTYPES; j++) {
        samples_current_epoch += thread_samples[i][j];
    }

    if (samples_current_epoch == 0) {
      zero_sample_epoch[i]++;
    } else {
      zero_sample_epoch[i] = 0;
    }

    if (zero_sample_epoch[i] >= zero_sample_epochs_threshold) {
      possibly_stuck[i] = true;
    }
  }

  // Count number of possible stuck threads
  int num_stuck_threads = 0;
  for (int i = 0; i < PEBS_NPROCS; i++) {
    if (possibly_stuck[i]) {
      num_stuck_threads++;
    }
  }

  if (num_stuck_threads == 0) {
    return;
  }

  if (num_stuck_threads < PEBS_NPROCS/2) {
    // Reset PEBS all stuck threads
    for (int i = 0; i < PEBS_NPROCS; i++) {
      if (possibly_stuck[i]) {
        zero_sample_epoch[i] = 0;

        fprintf(stderr, "Thread %d is possible stuck. Restarting PEBS.\n", i);

        for (int j = 0; j < NPBUFTYPES; j++) {
          ioctl(pfd[i][j], PERF_EVENT_IOC_DISABLE, 0);
          ioctl(pfd[i][j], PERF_EVENT_IOC_ENABLE, 0);
        }
      }
    }
  }
}

void *process_stats_periodic(void *args)
{
  struct timeval start, end;
  double elapsed_time;

  for (;;) {
    gettimeofday(&start, NULL);

#ifdef PEBS_PERIODIC
    // Accumulate page counts from all threads
    map<__u64, __u64> curr_epoch_counts;
    for (int i = 0; i < PEBS_NPROCS; i++) {
      for (auto& p : page_counts[i]) {
        auto it = curr_epoch_counts.find(p.first);
        if (it == curr_epoch_counts.end()) {
          curr_epoch_counts[p.first] = p.second;
        } else {
          curr_epoch_counts[p.first] += (p.second);
        }
      }
    }

    // Append curr_epoch_counts to total_page_counts
    for (auto& p : curr_epoch_counts) {
      auto it = total_page_counts.find(p.first);
      if (it == total_page_counts.end()) {
        total_page_counts[p.first] = vector<__u64>(epoch_cnt, 0);
      }
      total_page_counts[p.first].resize(epoch_cnt+1, 0);
      total_page_counts[p.first][epoch_cnt] = p.second;
    }
#endif

    epoch_cnt++;
    printf("Epoch %u\n", epoch_cnt);

    restart_stuck_threads();

    // Print number of samples per cpu per buffer type
    for (int i = 0; i < PEBS_NPROCS; i++) {
      printf("Thread %d:", i);
      for (int j = 0; j < NPBUFTYPES; j++) {
        printf(" %llu", thread_samples[i][j]);
        thread_samples[i][j] = 0;
      }
      printf("\n");
    }

    gettimeofday(&end, NULL);
    elapsed_time = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);
    if (elapsed_time < stats_period) {
      usleep(stats_period - elapsed_time);
    }
  }
}

void *pebs_scan_thread(void *args)
{
  int tid        = *(int *)args;
  int target_cpu = tid;
  int scan_cpu   = tid + SCANNING_THREAD_CPU;

  cpu_set_t cpuset;
  pthread_t thread;

  struct perf_sample* ps;

  thread = pthread_self();
  CPU_ZERO(&cpuset);
  CPU_SET(scan_cpu, &cpuset);
  int s = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
  if (s != 0) {
    perror("pthread_setaffinity_np");
    assert(0);
  }

  for(;;) {
    for(int j = 0; j < NPBUFTYPES; j++) {
      struct perf_event_mmap_page *p = perf_page[target_cpu][j];
      char *pbuf = (char *)p + p->data_offset;

      __sync_synchronize();

      if(p->data_head == p->data_tail) {
        continue;
      }

      struct perf_event_header *ph = (struct perf_event_header *)(pbuf + (p->data_tail % p->data_size));

      switch(ph->type) {
        case PERF_RECORD_SAMPLE:
          ps = (struct perf_sample*)ph;
          assert(ps != NULL);
          if(ps->addr != 0) {
            __u64 pfn = ps->addr >> PAGE_SHIFT;

            // Update page count
            auto it = page_counts[tid].find(pfn);
            if (it == page_counts[tid].end()) {
              page_counts[tid][pfn] = 1;
            } else {
              page_counts[tid][pfn]++;
            }
            thread_samples[tid][j]++;
          }
          break;
        case PERF_RECORD_THROTTLE:
          // fprintf(stderr, "Throttle\n");
          throttle_cnt++;
          break;
        case PERF_RECORD_UNTHROTTLE:
          // fprintf(stderr, "Unthrottle\n");
          unthrottle_cnt++;
          break;
        default:
          printf("Unknown type %u\n", ph->type);
          break;
      }

      p->data_tail += ph->size;
    }
  }

  return NULL;
}

void pebs_init(void)
{
  pthread_t stats_thread;
  pthread_t scan_threads[PEBS_NPROCS];
  // Args to the scan thread include the cpu number
  scan_thread_args = (int *)malloc(sizeof(int) * PEBS_NPROCS);

  for (int i = 0; i < PEBS_NPROCS; i++) {
#ifndef ENABLE_WRITE
    perf_page[i][DRAMREAD] = perf_setup(0x01d3, 0, i, DRAMREAD);      // MEM_LOAD_L3_MISS_RETIRED.LOCAL_DRAM
    perf_page[i][NVMREAD] = perf_setup(0x80d1, 0, i, NVMREAD);     // MEM_LOAD_L3_MISS_RETIRED.REMOTE_DRAM
#else
    perf_page[i][WRITE] = perf_setup(0x82d0, 0, i, WRITE);    // MEM_INST_RETIRED.ALL_STORES
#endif
    //perf_page[i][WRITE] = perf_setup(0x12d0, 0, i);   // MEM_INST_RETIRED.STLB_MISS_STORES

    scan_thread_args[i] = i;
    int r = pthread_create(&scan_threads[i], NULL, pebs_scan_thread, &scan_thread_args[i]);
    assert(r == 0);
  }

  stats_thread = pthread_create(&stats_thread, NULL, process_stats_periodic, NULL);

  signal(SIGINT, pebs_shutdown);
}

#ifdef PEBS_PERIODIC
void print_stats(void)
{
  // Print <page pfn, count0, count1, ...> tuples in to output file
  // Add 0s at the beginning if size of list is less than epoch_cnt
  for (auto& p : total_page_counts) {
    fprintf(output_fp, "%llx", p.first);
    __u64 last = 0;
    for (unsigned int j = 0; j < epoch_cnt; j++) {
      if (p.second[j] == 0) {
        fprintf(output_fp, " %llu", last);
      } else {
        fprintf(output_fp, " %llu", p.second[j]);
        last = p.second[j];
      }
    }
    fprintf(output_fp, "\n");
  }

  printf("Throttle cnt: %lu, Unthrottle cnt: %lu\n", throttle_cnt, unthrottle_cnt);
  printf("Page counts written to output file\n");
}
#else
void print_stats(void)
{
  map<__u64, __u64> total_page_counts; // Store the sum of page counts across all threads

  // Aggregate page counts across all threads
  for (int i = 0; i < PEBS_NPROCS; i++) {
    for (auto& p : page_counts[i]) {
      auto it = total_page_counts.find(p.first);
      if (it == total_page_counts.end()) {
        total_page_counts[p.first] = p.second;
      } else {
        total_page_counts[p.first] += p.second;
      }
    }
  }

  // Print <page pfn, count> tuples in to output file
  for (auto& p : total_page_counts) {
    fprintf(output_fp, "0x%llx %llu\n", p.first, p.second);
  }

  printf("Throttle cnt: %lu, Unthrottle cnt: %lu\n", throttle_cnt, unthrottle_cnt);
  printf("Page counts written to output file\n");
}
#endif

void pebs_shutdown(int sig_num)
{
  print_stats();

  for (int i = 0; i < PEBS_NPROCS; i++) {
    for (int j = 0; j < NPBUFTYPES; j++) {
      ioctl(pfd[i][j], PERF_EVENT_IOC_DISABLE, 0);
      //munmap(perf_page[i][j], sysconf(_SC_PAGESIZE) * PERF_PAGES);
    }
  }

  free(scan_thread_args);
}

int main(int argc, char* argv[])
{
  char *filename;
  int pipe_fd = -1;

  if (argc < 4) {
    printf("Usage: %s <sample_period> <stats_period> <output csv file> <pipe to quit>\n", argv[0]);
    return 1;
  }

  sample_period = atoi(argv[1]);
  stats_period  = atoi(argv[2]);
  filename = (char *)malloc(strlen(argv[3]) + 1);
  strncpy(filename, argv[3], strlen(argv[3]));
  // Check if we can open the output file
  output_fp = fopen(filename, "w");
  if (output_fp == NULL) {
    perror("fopen output_file");
    exit(EXIT_FAILURE);
  }

  // Open pipe
  if (argc > 4) {
	pipe_fd = open(argv[4], O_RDONLY | O_NONBLOCK);
    if (pipe_fd == -1) {
	  perror("open pipe");
	  exit(EXIT_FAILURE);
	}
  }

  pebs_init();

  printf("Enter 'q' to quit\n");
  while(1) {
    char userInput;

    // Set up the file descriptors for select to watch for user input
    fd_set set;
    FD_ZERO(&set);
    FD_SET(0, &set);

    // Wait for 1 second for user input
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    int ready = select(1, &set, NULL, NULL, &timeout);
    if (ready == -1) {
      perror("select");
      exit(EXIT_FAILURE);
    } else if (ready > 0) {
      if (1 == scanf("%c", &userInput)) {
        if (userInput == 'q') {
          break;
        }
      }
    }

    // Check the pipe for quit signal
    if (pipe_fd != -1) {
      char buf[1];
      if (read(pipe_fd, buf, 1) > 0) {
        if (buf[0] == 'q') {
          close(pipe_fd);
          break;
        }
      }
    }
  }

  pebs_shutdown(2);

  fclose(output_fp);

  return 0;
}