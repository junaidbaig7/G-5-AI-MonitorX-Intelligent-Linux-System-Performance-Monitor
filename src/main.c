#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <sys/stat.h>

#include "proc_stats.h"
#include "process_monitor.h"
#include "json_writer.h"
#include "python_bridge.h"
#include "dashboard.h"

#define OUTPUT_FILE "/tmp/ai_monitorx_stats.json"
#define CPU_LOG_FILE "logs/cpu.log"
#define MEM_LOG_FILE "logs/memory.log"
#define POLL_INTERVAL_SEC 2

#define CPU_THRESHOLD_PERCENT 80.0
#define MEM_THRESHOLD_PERCENT 85.0

/* sig_atomic_t is the only type it's safe to read/write from both a
 * signal handler and the main program without a race. Ctrl+C (or a
 * kill) sets this to 0; both the background thread and the menu loop
 * check it and shut down together. */
static volatile sig_atomic_t g_running = 1;

static void handle_shutdown_signal(int sig) {
    (void)sig;
    g_running = 0;
}

/* --------------------------------------------------------------------
 * Shared state: the background thread writes the latest reading here
 * every poll cycle; the menu (running on the main thread) reads it to
 * answer "CPU Usage" / "Memory Usage" / "Live Monitor" instantly. A
 * mutex protects it since two threads touch it concurrently.
 * ------------------------------------------------------------------*/
typedef struct {
    resource_snapshot_t cpu;
    resource_snapshot_t mem;
    int has_data;
} shared_state_t;

static shared_state_t g_state;
static pthread_mutex_t g_state_lock = PTHREAD_MUTEX_INITIALIZER;

static void get_current_state(shared_state_t *out) {
    pthread_mutex_lock(&g_state_lock);
    *out = g_state;
    pthread_mutex_unlock(&g_state_lock);
}

/* --------------------------------------------------------------------
 * Background thread: runs for the whole lifetime of the program,
 * independent of whatever the person is doing in the menu.
 *
 * Each cycle: read live /proc data (C's job), append it to the CSV
 * logs ai_prediction.py reads, then call that script to get the
 * predicted next value, trend, and threshold status (Python's job).
 * ------------------------------------------------------------------*/
static void *monitor_loop(void *arg) {
    (void)arg;

    cpu_raw_t prev_cpu, curr_cpu;
    int have_prev_cpu_sample = 0;
    int cpu_reading_count = 0;
    int mem_reading_count = 0;

    while (g_running) {
        /* --- CPU: needs two samples apart in time to get a % --- */
        double cpu_usage = 0.0;
        int cpu_sample_valid = 0;

        if (read_cpu_raw(&curr_cpu) == 0) {
            if (have_prev_cpu_sample) {
                cpu_usage = compute_cpu_usage(&prev_cpu, &curr_cpu);
                cpu_sample_valid = 1;
            }
            prev_cpu = curr_cpu;
            have_prev_cpu_sample = 1;
        }

        /* --- Memory: each reading is already a point-in-time % --- */
        mem_stats_t mem;
        if (read_mem_stats(&mem) != 0) {
            memset(&mem, 0, sizeof(mem));
        }

        /* --- Processes --- */
        static process_info_t all_procs[MAX_PROCESSES];
        int proc_count = list_processes(all_procs, MAX_PROCESSES);
        if (proc_count < 0) proc_count = 0;

        process_info_t top_procs[TOP_PROCESS_COUNT];
        int top_n = 0;
        if (proc_count > 0) {
            top_n = (proc_count < TOP_PROCESS_COUNT) ? proc_count : TOP_PROCESS_COUNT;
            get_top_by_memory(all_procs, proc_count, TOP_PROCESS_COUNT, top_procs);
        }

        /* --- Log this cycle's readings, then start both Python
         * predictions running concurrently (skip CPU on the very
         * first cycle -- there's no valid delta yet). Starting both
         * before reading either lets their ~1s pandas/sklearn cold
         * start overlap instead of stacking. --- */
        FILE *cpu_pipe = NULL;
        if (cpu_sample_valid) {
            cpu_reading_count++;
            log_reading(CPU_LOG_FILE, cpu_reading_count, cpu_usage);
            cpu_pipe = start_prediction("cpu");
        }

        mem_reading_count++;
        log_reading(MEM_LOG_FILE, mem_reading_count, mem.used_percent);
        FILE *mem_pipe = start_prediction("memory");

        /* Both python3 processes are now running in the background;
         * only now do we wait for their results. */
        resource_snapshot_t cpu_snap;
        finish_prediction(cpu_pipe, cpu_usage, CPU_THRESHOLD_PERCENT, &cpu_snap);

        resource_snapshot_t mem_snap;
        finish_prediction(mem_pipe, mem.used_percent, MEM_THRESHOLD_PERCENT, &mem_snap);

        pthread_mutex_lock(&g_state_lock);
        g_state.cpu = cpu_snap;
        g_state.mem = mem_snap;
        g_state.has_data = 1;
        pthread_mutex_unlock(&g_state_lock);

        write_snapshot_json(OUTPUT_FILE, &cpu_snap, &mem_snap,
                             top_procs, top_n, (long)time(NULL));

        sleep(POLL_INTERVAL_SEC);
    }

    return NULL;
}

/* --------------------------------------------------------------------
 * Menu actions (run on the main thread)
 * ------------------------------------------------------------------*/
static void show_cpu(void) {
    shared_state_t s;
    get_current_state(&s);
    if (!s.has_data) {
        printf("\nStill collecting the first sample -- try again in a couple seconds.\n");
        return;
    }
    print_resource_summary("CPU Usage", &s.cpu);
}

static void show_memory(void) {
    shared_state_t s;
    get_current_state(&s);
    if (!s.has_data) {
        printf("\nStill collecting the first sample -- try again in a couple seconds.\n");
        return;
    }
    print_resource_summary("Memory Usage", &s.mem);
}

static void show_processes(void) {
    static process_info_t procs[MAX_PROCESSES];
    int count = list_processes(procs, MAX_PROCESSES);
    if (count < 0) count = 0;

    printf("\n%-8s %-24s %12s\n", "PID", "NAME", "RSS (KB)");
    printf("--------------------------------------------------\n");

    if (count > 0) {
        process_info_t *sorted = malloc(sizeof(process_info_t) * (size_t)count);
        if (sorted) {
            get_top_by_memory(procs, count, count, sorted);
            for (int i = 0; i < count; i++) {
                printf("%-8d %-24s %12lu\n",
                       sorted[i].pid, sorted[i].name, sorted[i].vm_rss_kb);
            }
            free(sorted);
        }
    }
    printf("\nTotal processes: %d\n", count);
}

static void live_monitor(void) {
    printf("\nStarting live monitor...\n");
    printf("Press Ctrl+C to stop.\n\n");
    sleep(1);

    while (g_running) {
        shared_state_t s;
        get_current_state(&s);
        render_dashboard(&s.cpu, &s.mem);
        sleep(POLL_INTERVAL_SEC);
    }
}

int main(void) {
    /* ai_prediction.py expects logs/cpu.log and logs/memory.log to
     * exist; create the directory up front (ignore the error if it's
     * already there from a previous run). */
    mkdir("logs", 0755);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_shutdown_signal;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGINT, &sa, NULL) != 0 || sigaction(SIGTERM, &sa, NULL) != 0) {
        perror("sigaction");
        return 1;
    }

    pthread_t monitor_thread;
    if (pthread_create(&monitor_thread, NULL, monitor_loop, NULL) != 0) {
        fprintf(stderr, "[AI MonitorX] Failed to create monitoring thread\n");
        return 1;
    }

    int choice;

    while (g_running) {
        printf("\n");
        printf("=====================================\n");
        printf("          AI MONITORX                \n");
        printf("=====================================\n");
        printf("1. CPU Usage\n");
        printf("2. Memory Usage\n");
        printf("3. Running Processes\n");
        printf("4. Live Monitor\n");
        printf("5. Exit\n");
        printf("\nEnter your choice: ");
        fflush(stdout);

        int rc = scanf("%d", &choice);
        if (rc != 1) {
            if (!g_running) break; /* Ctrl+C interrupted the prompt itself */
            if (feof(stdin)) break; /* Ctrl+D */
            int c;
            while ((c = getchar()) != '\n' && c != EOF) { /* discard bad input */ }
            printf("\nInvalid input -- please enter a number.\n");
            continue;
        }

        switch (choice) {
            case 1: show_cpu(); break;
            case 2: show_memory(); break;
            case 3: show_processes(); break;
            case 4: live_monitor(); break;
            case 5:
                printf("\nExiting AI MonitorX...\n");
                g_running = 0;
                break;
            default:
                printf("\nInvalid choice.\n");
        }
    }

    g_running = 0; /* in case we got here via a signal, not option 5 */
    pthread_join(monitor_thread, NULL);

    printf("\n[AI MonitorX] Exited cleanly.\n");
    return 0;
}
