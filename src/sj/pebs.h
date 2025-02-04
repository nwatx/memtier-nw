#ifndef PEBS_H
#define PEBS_H

#include <pthread.h>
#include <stdint.h>
#include <inttypes.h>
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>


#define PERF_PAGES	(1 + (1 << 16))	// Has to be == 1+2^n, here 1MB
#define SAMPLE_PERIOD	500
#define WRITE_SAMPLE_PERIOD	3000

// Change these macros based on the machine and workload config
#define START_CPU (0)
#define PEBS_NPROCS         (8)  // Number of cores
#define SCANNING_THREAD_CPU (8)

#define PAGE_SHIFT (21)
#define PAGE_SIZE (1 << PAGE_SHIFT)

#define MAX_EPOCHS (30)

struct perf_sample {
    struct perf_event_header header;
    __u64	ip;
    __u32 pid, tid;    /* if PERF_SAMPLE_TID */
    __u64 addr;        /* if PERF_SAMPLE_ADDR */
    // __u64 weight;      /* if PERF_SAMPLE_WEIGHT */
    /* __u64 data_src;    /\* if PERF_SAMPLE_DATA_SRC *\/ */
};

enum pbuftype {
#ifndef ENABLE_WRITE
    DRAMREAD,
    NVMREAD,
  #else
    WRITE,
  #endif
    NPBUFTYPES
  };

void *pebs_kswapd();
struct hemem_page* pebs_pagefault(void);
struct hemem_page* pebs_pagefault_unlocked(void);
void pebs_init(void);
void pebs_remove_page(struct hemem_page *page);
void pebs_stats();
void pebs_shutdown(int sig_num);

#endif /*  PEBS_H  */