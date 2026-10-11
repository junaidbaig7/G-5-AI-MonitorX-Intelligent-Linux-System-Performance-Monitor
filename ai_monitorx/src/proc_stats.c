#include <stdio.h>
#include <string.h>
#include "proc_stats.h"

/*
 * /proc/stat's first line looks like:
 *   cpu  132453 231 45324 9812345 1234 0 543 0 0 0
 * fields (in order): user nice system idle iowait irq softirq steal ...
 * All values are in USER_HZ ticks since boot.
 */
int read_cpu_raw(cpu_raw_t *out) {
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) {
        return -1;
    }

    char label[16];
    int n = fscanf(fp, "%15s %llu %llu %llu %llu %llu %llu %llu %llu",
                    label,
                    &out->user, &out->nice, &out->system, &out->idle,
                    &out->iowait, &out->irq, &out->softirq, &out->steal);
    fclose(fp);

    /* label + at least user/nice/system/idle must be present */
    if (n < 5) {
        return -1;
    }
    return 0;
}

double compute_cpu_usage(const cpu_raw_t *prev, const cpu_raw_t *curr) {
    unsigned long long prev_idle = prev->idle + prev->iowait;
    unsigned long long curr_idle = curr->idle + curr->iowait;

    unsigned long long prev_active = prev->user + prev->nice + prev->system +
                                      prev->irq + prev->softirq + prev->steal;
    unsigned long long curr_active = curr->user + curr->nice + curr->system +
                                      curr->irq + curr->softirq + curr->steal;

    unsigned long long prev_total = prev_idle + prev_active;
    unsigned long long curr_total = curr_idle + curr_active;

    double total_delta = (double)(curr_total - prev_total);
    double idle_delta = (double)(curr_idle - prev_idle);

    if (total_delta <= 0.0) {
        return 0.0;
    }
    return ((total_delta - idle_delta) / total_delta) * 100.0;
}

/*
 * /proc/meminfo has one "Key:   value kB" pair per line. We only
 * care about a handful of keys, so we just try to match each line
 * against each field we want.
 */
int read_mem_stats(mem_stats_t *out) {
    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    char line[256];

    while (fgets(line, sizeof(line), fp)) {
        sscanf(line, "MemTotal: %lu kB", &out->total_kb);
        sscanf(line, "MemFree: %lu kB", &out->free_kb);
        sscanf(line, "MemAvailable: %lu kB", &out->available_kb);
        sscanf(line, "Buffers: %lu kB", &out->buffers_kb);
        sscanf(line, "Cached: %lu kB", &out->cached_kb);
    }
    fclose(fp);

    if (out->total_kb > 0) {
        unsigned long used_kb = out->total_kb - out->available_kb;
        out->used_percent = ((double)used_kb / (double)out->total_kb) * 100.0;
    }
    return 0;
}
