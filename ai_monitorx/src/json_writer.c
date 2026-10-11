#include <stdio.h>
#include <string.h>
#include "json_writer.h"

/* Process names almost never contain quotes or backslashes, but if
 * one did, it would produce invalid JSON -- so escape defensively
 * rather than trust /proc's output. */
static void write_escaped_string(FILE *fp, const char *s) {
    fputc('"', fp);
    for (const char *p = s; *p; p++) {
        if (*p == '"' || *p == '\\') {
            fputc('\\', fp);
        }
        fputc(*p, fp);
    }
    fputc('"', fp);
}

static void write_resource_block(FILE *fp, const char *name,
                                  const resource_snapshot_t *r) {
    fprintf(fp, "  \"%s\": {\n", name);
    fprintf(fp, "    \"current_percent\": %.2f,\n", r->current_percent);
    fprintf(fp, "    \"predicted_percent\": %.2f,\n", r->predicted_percent);
    fprintf(fp, "    \"threshold_percent\": %.2f,\n", r->threshold_percent);
    fprintf(fp, "    \"trend\": \"%s\",\n", r->trend);
    fprintf(fp, "    \"status\": \"%s\"\n",
            r->over_threshold ? "warning" : "normal");
    fprintf(fp, "  },\n");
}

int write_snapshot_json(const char *filepath,
                         const resource_snapshot_t *cpu,
                         const resource_snapshot_t *mem,
                         const process_info_t *top_procs,
                         int top_count,
                         long timestamp)
{
    char tmp_path[280];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", filepath);

    FILE *fp = fopen(tmp_path, "w");
    if (!fp) return -1;

    fprintf(fp, "{\n");
    fprintf(fp, "  \"timestamp\": %ld,\n", timestamp);
    write_resource_block(fp, "cpu", cpu);
    write_resource_block(fp, "memory", mem);
    fprintf(fp, "  \"top_processes\": [\n");

    for (int i = 0; i < top_count; i++) {
        fprintf(fp, "    { \"pid\": %d, \"name\": ", top_procs[i].pid);
        write_escaped_string(fp, top_procs[i].name);
        fprintf(fp, ", \"rss_kb\": %lu }%s\n",
                top_procs[i].vm_rss_kb,
                (i == top_count - 1) ? "" : ",");
    }

    fprintf(fp, "  ]\n");
    fprintf(fp, "}\n");
    fclose(fp);

    if (rename(tmp_path, filepath) != 0) {
        return -1;
    }
    return 0;
}
