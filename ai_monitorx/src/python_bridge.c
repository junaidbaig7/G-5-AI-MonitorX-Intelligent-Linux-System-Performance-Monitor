#include <stdio.h>
#include <string.h>
#include "python_bridge.h"

void log_reading(const char *filepath, int reading_number, double value) {
    FILE *fp;

    if (reading_number == 1) {
        fp = fopen(filepath, "w");
        if (fp) {
            fprintf(fp, "reading,usage\n");
        }
    } else {
        fp = fopen(filepath, "a");
    }

    if (!fp) return;
    fprintf(fp, "%d,%.2f\n", reading_number, value);
    fclose(fp);
}

static void fill_defaults(resource_snapshot_t *out, double current_value,
                           double threshold) {
    out->current_percent = current_value;
    out->predicted_percent = current_value;
    out->threshold_percent = threshold;
    strncpy(out->trend, "stable", sizeof(out->trend) - 1);
    out->trend[sizeof(out->trend) - 1] = '\0';
    out->over_threshold = 0;
}

FILE *start_prediction(const char *resource_name) {
    char cmd[256];
    /* stderr redirected away: if pandas/sklearn print a warning, it
     * must not land in the single line we're about to parse. popen()
     * forks and starts this running in the background immediately --
     * it does not block until the caller reads from the pipe. */
    snprintf(cmd, sizeof(cmd),
             "python3 ai_prediction.py %s --for-c 2>/dev/null",
             resource_name);
    return popen(cmd, "r");
}

int finish_prediction(FILE *pipe, double current_value, double threshold,
                       resource_snapshot_t *out) {
    fill_defaults(out, current_value, threshold); /* safe baseline */

    if (!pipe) {
        return 0;
    }

    char line[256];
    int got_line = (fgets(line, sizeof(line), pipe) != NULL);
    pclose(pipe);

    if (!got_line || strncmp(line, "NA", 2) == 0) {
        return 0; /* Python didn't have 5 readings yet -- not an error */
    }

    double parsed_current, parsed_predicted, parsed_threshold;
    char parsed_trend[16];
    int parsed_over;

    int n = sscanf(line, "%lf,%lf,%lf,%15[^,],%d",
                    &parsed_current, &parsed_predicted, &parsed_threshold,
                    parsed_trend, &parsed_over);
    if (n != 5) {
        return 0; /* unexpected output -- keep the safe defaults */
    }

    out->current_percent = current_value; /* trust our own live reading */
    out->predicted_percent = parsed_predicted;
    out->threshold_percent = threshold;
    strncpy(out->trend, parsed_trend, sizeof(out->trend) - 1);
    out->trend[sizeof(out->trend) - 1] = '\0';
    out->over_threshold = parsed_over;

    return 1;
}
