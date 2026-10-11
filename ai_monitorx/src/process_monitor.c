#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include "process_monitor.h"

static int is_all_digits(const char *s) {
    if (*s == '\0') return 0;
    while (*s) {
        if (!isdigit((unsigned char)*s)) return 0;
        s++;
    }
    return 1;
}

/*
 * /proc/[pid]/stat is one line, space-separated, e.g.:
 *   1234 (bash) S 1000 1234 1234 34816 1234 4194304 1231 ...
 * Field 2 (comm) is wrapped in parentheses and can itself contain
 * spaces, so we read it with %[^)] instead of plain %s.
 * Field 3 is state. Fields 14 and 15 (after state) are utime/stime,
 * so we skip 11 unsigned longs to get there.
 */
static void read_process_stat(int pid, process_info_t *info) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);

    FILE *fp = fopen(path, "r");
    if (!fp) return;

    char comm[256];
    char state;
    int scanned_pid;

    if (fscanf(fp, "%d (%255[^)]) %c", &scanned_pid, comm, &state) != 3) {
        fclose(fp);
        return;
    }

    unsigned long dummy;
    for (int i = 0; i < 11; i++) {
        if (fscanf(fp, "%lu", &dummy) != 1) {
            fclose(fp);
            return;
        }
    }

    unsigned long utime, stime;
    if (fscanf(fp, "%lu %lu", &utime, &stime) != 2) {
        fclose(fp);
        return;
    }
    fclose(fp);

    info->pid = scanned_pid;
    info->state = state;
    info->utime = utime;
    info->stime = stime;
    strncpy(info->name, comm, MAX_NAME_LEN - 1);
    info->name[MAX_NAME_LEN - 1] = '\0';
}

static unsigned long read_process_rss(int pid) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/status", pid);

    FILE *fp = fopen(path, "r");
    if (!fp) return 0;

    unsigned long rss_kb = 0;
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "VmRSS: %lu kB", &rss_kb) == 1) {
            break;
        }
    }
    fclose(fp);
    return rss_kb;
}

int list_processes(process_info_t *arr, int max_count) {
    DIR *proc_dir = opendir("/proc");
    if (!proc_dir) return -1;

    struct dirent *entry;
    int count = 0;

    while (count < max_count && (entry = readdir(proc_dir)) != NULL) {
        if (!is_all_digits(entry->d_name)) continue;

        int pid = atoi(entry->d_name);
        process_info_t info;
        memset(&info, 0, sizeof(info));
        info.pid = pid;
        strncpy(info.name, "?", MAX_NAME_LEN - 1);

        /* A process can exit between readdir() and us reading its
         * files -- that's normal and not an error, we just skip it
         * by leaving whatever partial info we got. */
        read_process_stat(pid, &info);
        info.vm_rss_kb = read_process_rss(pid);

        arr[count++] = info;
    }

    closedir(proc_dir);
    return count;
}

static int compare_by_rss_desc(const void *a, const void *b) {
    const process_info_t *pa = (const process_info_t *)a;
    const process_info_t *pb = (const process_info_t *)b;
    if (pb->vm_rss_kb > pa->vm_rss_kb) return 1;
    if (pb->vm_rss_kb < pa->vm_rss_kb) return -1;
    return 0;
}

void get_top_by_memory(process_info_t *arr, int count, int top_n,
                        process_info_t *out) {
    if (count <= 0) return;

    process_info_t *copy = malloc(sizeof(process_info_t) * (size_t)count);
    if (!copy) return;

    memcpy(copy, arr, sizeof(process_info_t) * (size_t)count);
    qsort(copy, (size_t)count, sizeof(process_info_t), compare_by_rss_desc);

    int n = (top_n < count) ? top_n : count;
    memcpy(out, copy, sizeof(process_info_t) * (size_t)n);

    free(copy);
}
