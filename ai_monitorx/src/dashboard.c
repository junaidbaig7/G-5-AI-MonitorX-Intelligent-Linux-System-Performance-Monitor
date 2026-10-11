#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "dashboard.h"

#define BOX_WIDTH 37 /* visible columns between the two vertical borders */

/* Python (ai_prediction.py) decides the trend classification; this
 * just adds the arrow for terminal display. Keeps classification
 * logic in one place (Python) and presentation in another (here). */
static const char *format_trend(const char *trend) {
    if (strcmp(trend, "increasing") == 0) return "↑ Increasing";
    if (strcmp(trend, "decreasing") == 0) return "↓ Decreasing";
    return "→ Stable";
}


/* Counts Unicode codepoints (not bytes) in a UTF-8 string, so padding
 * math lines up even when a value contains a multi-byte arrow/symbol
 * like "↑" or "⚠". Continuation bytes (10xxxxxx) don't count. */
static int utf8_len(const char *s) {
    int len = 0;
    for (; *s; s++) {
        if (((unsigned char)*s & 0xC0) != 0x80) {
            len++;
        }
    }
    return len;
}

static void print_border(const char *left, const char *right) {
    printf("%s", left);
    for (int i = 0; i < BOX_WIDTH; i++) {
        printf("─");
    }
    printf("%s\n", right);
}

static void print_title(const char *title) {
    int pad = BOX_WIDTH - (int)strlen(title);
    if (pad < 0) pad = 0;
    int left = pad / 2;
    int right = pad - left;
    printf("│%*s%s%*s│\n", left, "", title, right, "");
}

static void print_section(const char *name) {
    printf("│ %-*s│\n", BOX_WIDTH - 1, name);
}

static void print_blank(void) {
    printf("│%*s│\n", BOX_WIDTH, "");
}

/* One "Label    value" row, e.g. "Current       76%". label_field
 * controls how wide the label column is before the value starts. */
static void print_row(const char *label, const char *value) {
    const int label_field = 14;
    char content[256];

    int label_len = (int)strlen(label);
    int label_pad = label_field - label_len;
    if (label_pad < 0) label_pad = 0;

    snprintf(content, sizeof(content), " %s%*s%s", label, label_pad, "", value);

    int content_len = utf8_len(content);
    int right_pad = BOX_WIDTH - content_len;
    if (right_pad < 0) right_pad = 0;

    printf("│%s%*s│\n", content, right_pad, "");
}

static void render_resource(const char *name, const resource_snapshot_t *r) {
    char current_str[32], predicted_str[32], threshold_str[32], status_str[48];
    snprintf(current_str, sizeof(current_str), "%.0f%%", r->current_percent);
    snprintf(predicted_str, sizeof(predicted_str), "%.0f%%", r->predicted_percent);
    snprintf(threshold_str, sizeof(threshold_str), "%.0f%%", r->threshold_percent);

    if (r->over_threshold) {
        snprintf(status_str, sizeof(status_str), "⚠ HIGH %s EXPECTED", name);
    } else {
        snprintf(status_str, sizeof(status_str), "✓ NORMAL");
    }

    print_section(name);
    print_blank();
    print_row("Current", current_str);
    print_row("Predicted", predicted_str);
    print_row("Threshold", threshold_str);
    print_row("Trend", format_trend(r->trend));
    print_row("Status", status_str);
}

void render_dashboard(const resource_snapshot_t *cpu, const resource_snapshot_t *mem) {
    /* Only clear the screen for a real interactive terminal -- if
     * stdout is redirected to a file or pipe, clearing would just
     * fill the log with unreadable escape codes. */
    if (isatty(STDOUT_FILENO)) {
        printf("\033[2J\033[H");
    }

    print_border("┌", "┐");
    print_title("AI MONITORX");
    print_border("├", "┤");
    render_resource("CPU", cpu);
    print_border("├", "┤");
    render_resource("MEMORY", mem);
    print_border("└", "┘");
    fflush(stdout);
}

void print_resource_summary(const char *title, const resource_snapshot_t *r) {
    printf("\n%s\n", title);
    printf("-----------------------------\n");
    printf("Current      : %.2f%%\n", r->current_percent);
    printf("Predicted    : %.2f%%\n", r->predicted_percent);
    printf("Threshold    : %.2f%%\n", r->threshold_percent);
    printf("Trend        : %s\n", format_trend(r->trend));
    printf("Status       : %s\n",
           r->over_threshold ? "⚠ WARNING - threshold likely to be exceeded"
                              : "✓ NORMAL");
}
