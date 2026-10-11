#ifndef JSON_WRITER_H
#define JSON_WRITER_H

#include "proc_stats.h"
#include "process_monitor.h"

#define TOP_PROCESS_COUNT 5

/* Everything the dashboard (terminal box, and later the React GUI)
 * needs to describe one resource (CPU or memory) at a point in time.
 * current_percent comes from C's own /proc reading; predicted_percent,
 * trend and over_threshold come from ai_prediction.py. */
typedef struct {
    double current_percent;
    double predicted_percent;
    double threshold_percent;
    char trend[16]; /* "increasing" | "decreasing" | "stable" */
    int over_threshold;
} resource_snapshot_t;

/* Writes one JSON snapshot to filepath. Writes to a temporary file
 * first and renames it into place, so a reader (e.g. Flask) never
 * sees a half-written file mid-update. Returns 0 on success. */
int write_snapshot_json(const char *filepath,
                         const resource_snapshot_t *cpu,
                         const resource_snapshot_t *mem,
                         const process_info_t *top_procs,
                         int top_count,
                         long timestamp);

#endif /* JSON_WRITER_H */
