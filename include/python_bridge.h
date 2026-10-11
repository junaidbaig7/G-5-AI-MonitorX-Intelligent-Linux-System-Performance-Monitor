#ifndef PYTHON_BRIDGE_H
#define PYTHON_BRIDGE_H

#include <stdio.h>
#include "json_writer.h"

/* Appends one reading to filepath as a CSV row, in the format
 * ai_prediction.py expects (columns: reading, usage). If
 * reading_number is 1, the file is (re)created with a header row
 * first, so each run of the program starts a fresh log. */
void log_reading(const char *filepath, int reading_number, double value);

/* Starts `python3 ai_prediction.py <resource_name> --for-c` running
 * in the background and returns its output pipe immediately, without
 * waiting for it to finish. Cold-starting Python (importing pandas +
 * sklearn) takes about a second, so starting the CPU and memory
 * predictions back-to-back lets both run concurrently instead of
 * one after another -- roughly halving the total wait per cycle.
 * Returns NULL on failure to launch. */
FILE *start_prediction(const char *resource_name);

/* Waits for a pipe returned by start_prediction(), parses its
 * output, and fills *out. current_percent is always set from
 * current_value (the live /proc reading), regardless of whether
 * Python had enough history yet. Returns 1 if Python returned a real
 * prediction, 0 if there wasn't enough data yet or the pipe was NULL
 * (out is still filled with safe defaults in that case). */
int finish_prediction(FILE *pipe, double current_value, double threshold,
                       resource_snapshot_t *out);

#endif /* PYTHON_BRIDGE_H */
