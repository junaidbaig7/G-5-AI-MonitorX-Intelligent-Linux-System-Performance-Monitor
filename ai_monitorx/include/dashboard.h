#ifndef DASHBOARD_H
#define DASHBOARD_H

#include "json_writer.h"

/* Renders the boxed CPU + Memory dashboard to stdout. If stdout is a
 * real terminal, it also clears the screen first so it behaves like
 * a live-updating panel; if stdout is redirected to a file/pipe, it
 * skips clearing so the output stays readable as a plain log. */
void render_dashboard(const resource_snapshot_t *cpu,
                       const resource_snapshot_t *mem);

/* Prints a plain-text summary of a single resource, for the menu's
 * quick "CPU Usage" / "Memory Usage" options. */
void print_resource_summary(const char *title, const resource_snapshot_t *r);

#endif /* DASHBOARD_H */
