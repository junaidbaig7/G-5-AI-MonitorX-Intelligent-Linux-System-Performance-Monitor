#ifndef PROC_STATS_H
#define PROC_STATS_H

/* Raw cumulative CPU tick counts, straight from /proc/stat.
 * These are totals since boot, not percentages -- you need two
 * samples taken apart in time to compute a usage percentage. */
typedef struct {
    unsigned long long user;
    unsigned long long nice;
    unsigned long long system;
    unsigned long long idle;
    unsigned long long iowait;
    unsigned long long irq;
    unsigned long long softirq;
    unsigned long long steal;
} cpu_raw_t;

typedef struct {
    unsigned long total_kb;
    unsigned long free_kb;
    unsigned long available_kb;
    unsigned long buffers_kb;
    unsigned long cached_kb;
    double used_percent;
} mem_stats_t;

/* Reads the first line ("cpu  ...") of /proc/stat into *out.
 * Returns 0 on success, -1 on failure. */
int read_cpu_raw(cpu_raw_t *out);

/* Given two samples taken apart in time, returns CPU usage as a
 * percentage (0.0 - 100.0) over that interval. */
double compute_cpu_usage(const cpu_raw_t *prev, const cpu_raw_t *curr);

/* Parses /proc/meminfo into *out, including a derived used_percent.
 * Returns 0 on success, -1 on failure. */
int read_mem_stats(mem_stats_t *out);

#endif /* PROC_STATS_H */
