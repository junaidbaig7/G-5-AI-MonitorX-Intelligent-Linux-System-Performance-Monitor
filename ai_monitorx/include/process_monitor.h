#ifndef PROCESS_MONITOR_H
#define PROCESS_MONITOR_H

#define MAX_PROCESSES 1024
#define MAX_NAME_LEN 64

typedef struct {
    int pid;
    char name[MAX_NAME_LEN];
    char state;              /* 'R' running, 'S' sleeping, 'Z' zombie, ... */
    unsigned long vm_rss_kb; /* resident memory, from /proc/[pid]/status */
    unsigned long utime;     /* user-mode CPU ticks, from /proc/[pid]/stat */
    unsigned long stime;     /* kernel-mode CPU ticks */
} process_info_t;

/* Scans /proc for numeric directories (one per running process),
 * fills arr[] with up to max_count entries.
 * Returns the number of processes found, or -1 on failure. */
int list_processes(process_info_t *arr, int max_count);

/* Copies the top_n processes by resident memory (descending) from
 * arr[0..count) into out[0..top_n). Caller must ensure out has room
 * for at least min(top_n, count) entries. */
void get_top_by_memory(process_info_t *arr, int count, int top_n,
                        process_info_t *out);

#endif /* PROCESS_MONITOR_H */
