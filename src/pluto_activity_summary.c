#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char timestamp[64];
    int cycle;
    double target_hz;
    double tune_lo_hz;
    double if_offset_hz;
    double best_offset_hz;
    double best_freq_hz;
    double power_db;
    double noise_floor_db;
    double snr_db;
    int active;
} activity_row_t;

typedef struct {
    double target_hz;

    int total_checks;
    int active_hits;

    char first_seen[64];
    char last_seen[64];
    char first_active[64];
    char last_active[64];

    double sum_snr_db;
    double sum_active_snr_db;

    double max_snr_db;
    double min_snr_db;

    double best_freq_hz;
    double best_power_db;
    double best_noise_floor_db;

    bool initialized;
    bool has_active;
} freq_summary_t;

typedef struct {
    const char *input_file;
    const char *output_file;

    double merge_hz;
    int min_hits;
    double min_percent_active;

    bool sort_by_activity;
} app_config_t;

static void print_usage(const char *prog)
{
    printf("\n");
    printf("Pluto+ Activity Summary Tool\n");
    printf("\n");
    printf("Usage:\n");
    printf("  %s --in <activity.csv> [options]\n", prog);
    printf("\n");
    printf("Options:\n");
    printf("  --in <file>             Input CSV from pluto_activity_monitor\n");
    printf("  --out <file>            Output summary CSV, default activity_summary.csv\n");
    printf("  --merge-hz <hz>         Merge target frequencies within this spacing, default 1\n");
    printf("  --min-hits <n>          Only show frequencies with at least n active hits, default 0\n");
    printf("  --min-active-pct <pct>  Only show frequencies active at least pct percent, default 0\n");
    printf("  --sort <mode>           freq or activity, default freq\n");
    printf("  --help                  Show this help\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s --in 2m_activity.csv --out 2m_activity_summary.csv\n", prog);
    printf("  %s --in airband_activity.csv --min-hits 2 --sort activity\n", prog);
    printf("  %s --in noaa_activity.csv --min-active-pct 50\n", prog);
    printf("\n");
}

static bool parse_double_value(const char *text, double *value)
{
    char *end = NULL;
    errno = 0;

    double v = strtod(text, &end);

    if (errno != 0 || end == text || *end != '\0') {
        return false;
    }

    *value = v;
    return true;
}

static bool parse_int_value(const char *text, int *value)
{
    char *end = NULL;
    errno = 0;

    long v = strtol(text, &end, 10);

    if (errno != 0 || end == text || *end != '\0') {
        return false;
    }

    *value = (int)v;
    return true;
}

static bool parse_args(int argc, char **argv, app_config_t *cfg)
{
    cfg->input_file = NULL;
    cfg->output_file = "activity_summary.csv";

    cfg->merge_hz = 1.0;
    cfg->min_hits = 0;
    cfg->min_percent_active = 0.0;

    cfg->sort_by_activity = false;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];

        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            print_usage(argv[0]);
            exit(0);
        } else if (strcmp(arg, "--in") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "ERROR: --in requires a filename\n");
                return false;
            }
            cfg->input_file = argv[i];
        } else if (strcmp(arg, "--out") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "ERROR: --out requires a filename\n");
                return false;
            }
            cfg->output_file = argv[i];
        } else if (strcmp(arg, "--merge-hz") == 0) {
            if (++i >= argc || !parse_double_value(argv[i], &cfg->merge_hz)) {
                fprintf(stderr, "ERROR: --merge-hz requires a numeric value\n");
                return false;
            }
        } else if (strcmp(arg, "--min-hits") == 0) {
            if (++i >= argc || !parse_int_value(argv[i], &cfg->min_hits)) {
                fprintf(stderr, "ERROR: --min-hits requires an integer value\n");
                return false;
            }
        } else if (strcmp(arg, "--min-active-pct") == 0) {
            if (++i >= argc || !parse_double_value(argv[i], &cfg->min_percent_active)) {
                fprintf(stderr, "ERROR: --min-active-pct requires a numeric percent value\n");
                return false;
            }
        } else if (strcmp(arg, "--sort") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "ERROR: --sort requires freq or activity\n");
                return false;
            }

            if (strcmp(argv[i], "freq") == 0) {
                cfg->sort_by_activity = false;
            } else if (strcmp(argv[i], "activity") == 0) {
                cfg->sort_by_activity = true;
            } else {
                fprintf(stderr, "ERROR: --sort must be freq or activity\n");
                return false;
            }
        } else {
            fprintf(stderr, "ERROR: Unknown option: %s\n", arg);
            return false;
        }
    }

    if (!cfg->input_file) {
        fprintf(stderr, "ERROR: --in is required\n");
        return false;
    }

    if (cfg->merge_hz < 0.0) {
        fprintf(stderr, "ERROR: --merge-hz must not be negative\n");
        return false;
    }

    if (cfg->min_hits < 0) {
        fprintf(stderr, "ERROR: --min-hits must not be negative\n");
        return false;
    }

    if (cfg->min_percent_active < 0.0 || cfg->min_percent_active > 100.0) {
        fprintf(stderr, "ERROR: --min-active-pct must be between 0 and 100\n");
        return false;
    }

    return true;
}

static int compare_summary_freq(const void *a, const void *b)
{
    const freq_summary_t *sa = (const freq_summary_t *)a;
    const freq_summary_t *sb = (const freq_summary_t *)b;

    if (sa->target_hz < sb->target_hz) {
        return -1;
    }

    if (sa->target_hz > sb->target_hz) {
        return 1;
    }

    return 0;
}

static double percent_active(const freq_summary_t *s)
{
    if (s->total_checks <= 0) {
        return 0.0;
    }

    return 100.0 * (double)s->active_hits / (double)s->total_checks;
}

static int compare_summary_activity_desc(const void *a, const void *b)
{
    const freq_summary_t *sa = (const freq_summary_t *)a;
    const freq_summary_t *sb = (const freq_summary_t *)b;

    double pa = percent_active(sa);
    double pb = percent_active(sb);

    if (pa > pb) {
        return -1;
    }

    if (pa < pb) {
        return 1;
    }

    if (sa->max_snr_db > sb->max_snr_db) {
        return -1;
    }

    if (sa->max_snr_db < sb->max_snr_db) {
        return 1;
    }

    return compare_summary_freq(a, b);
}

static bool parse_activity_line(char *line, activity_row_t *row)
{
    /*
       Expected:
       timestamp,cycle,target_hz,tune_lo_hz,if_offset_hz,best_offset_hz,best_freq_hz,power_db,noise_floor_db,snr_db,active
    */

    char *fields[11] = {0};
    int count = 0;

    char *token = strtok(line, ",");

    while (token && count < 11) {
        fields[count++] = token;
        token = strtok(NULL, ",");
    }

    if (count != 11) {
        return false;
    }

    snprintf(row->timestamp, sizeof(row->timestamp), "%s", fields[0]);

    if (!parse_int_value(fields[1], &row->cycle)) {
        return false;
    }

    if (!parse_double_value(fields[2], &row->target_hz)) {
        return false;
    }

    if (!parse_double_value(fields[3], &row->tune_lo_hz)) {
        return false;
    }

    if (!parse_double_value(fields[4], &row->if_offset_hz)) {
        return false;
    }

    if (!parse_double_value(fields[5], &row->best_offset_hz)) {
        return false;
    }

    if (!parse_double_value(fields[6], &row->best_freq_hz)) {
        return false;
    }

    if (!parse_double_value(fields[7], &row->power_db)) {
        return false;
    }

    if (!parse_double_value(fields[8], &row->noise_floor_db)) {
        return false;
    }

    if (!parse_double_value(fields[9], &row->snr_db)) {
        return false;
    }

    if (!parse_int_value(fields[10], &row->active)) {
        return false;
    }

    return true;
}

static bool append_summary(freq_summary_t **items, size_t *count, size_t *capacity, const freq_summary_t *summary)
{
    if (*count >= *capacity) {
        size_t new_capacity = (*capacity == 0) ? 64 : (*capacity * 2);

        freq_summary_t *new_items = (freq_summary_t *)realloc(*items, new_capacity * sizeof(freq_summary_t));
        if (!new_items) {
            return false;
        }

        *items = new_items;
        *capacity = new_capacity;
    }

    (*items)[*count] = *summary;
    (*count)++;

    return true;
}

static freq_summary_t *find_or_create_summary(
    freq_summary_t **items,
    size_t *count,
    size_t *capacity,
    double target_hz,
    double merge_hz)
{
    for (size_t i = 0; i < *count; i++) {
        double diff = (*items)[i].target_hz - target_hz;

        if (diff < 0.0) {
            diff = -diff;
        }

        if (diff <= merge_hz) {
            return &(*items)[i];
        }
    }

    freq_summary_t s;
    memset(&s, 0, sizeof(s));

    s.target_hz = target_hz;
    s.max_snr_db = -1e300;
    s.min_snr_db = 1e300;
    s.best_freq_hz = 0.0;
    s.best_power_db = 0.0;
    s.best_noise_floor_db = 0.0;
    s.initialized = true;
    s.has_active = false;

    if (!append_summary(items, count, capacity, &s)) {
        return NULL;
    }

    return &(*items)[(*count) - 1];
}

static void update_summary(freq_summary_t *s, const activity_row_t *row)
{
    if (s->total_checks == 0) {
        snprintf(s->first_seen, sizeof(s->first_seen), "%s", row->timestamp);
        snprintf(s->last_seen, sizeof(s->last_seen), "%s", row->timestamp);
    } else {
        snprintf(s->last_seen, sizeof(s->last_seen), "%s", row->timestamp);
    }

    s->total_checks++;
    s->sum_snr_db += row->snr_db;

    if (row->snr_db > s->max_snr_db) {
        s->max_snr_db = row->snr_db;
        s->best_freq_hz = row->best_freq_hz;
        s->best_power_db = row->power_db;
        s->best_noise_floor_db = row->noise_floor_db;
    }

    if (row->snr_db < s->min_snr_db) {
        s->min_snr_db = row->snr_db;
    }

    if (row->active != 0) {
        if (!s->has_active) {
            snprintf(s->first_active, sizeof(s->first_active), "%s", row->timestamp);
            snprintf(s->last_active, sizeof(s->last_active), "%s", row->timestamp);
            s->has_active = true;
        } else {
            snprintf(s->last_active, sizeof(s->last_active), "%s", row->timestamp);
        }

        s->active_hits++;
        s->sum_active_snr_db += row->snr_db;
    }
}

int main(int argc, char **argv)
{
    app_config_t cfg;

    if (!parse_args(argc, argv, &cfg)) {
        print_usage(argv[0]);
        return 1;
    }

    printf("\n");
    printf("Pluto+ Activity Summary Tool\n");
    printf("----------------------------\n");
    printf("Input CSV:        %s\n", cfg.input_file);
    printf("Output CSV:       %s\n", cfg.output_file);
    printf("Merge Hz:         %.1f\n", cfg.merge_hz);
    printf("Minimum hits:     %d\n", cfg.min_hits);
    printf("Minimum active:   %.1f%%\n", cfg.min_percent_active);
    printf("Sort:             %s\n", cfg.sort_by_activity ? "activity" : "freq");
    printf("\n");

    FILE *in = fopen(cfg.input_file, "r");
    if (!in) {
        fprintf(stderr, "ERROR: Could not open input CSV: %s\n", cfg.input_file);
        return 1;
    }

    freq_summary_t *summaries = NULL;
    size_t summary_count = 0;
    size_t summary_capacity = 0;

    char line[2048];
    long line_number = 0;
    long rows_read = 0;

    while (fgets(line, sizeof(line), in)) {
        line_number++;

        if (line_number == 1 && strstr(line, "timestamp") != NULL) {
            continue;
        }

        line[strcspn(line, "\r\n")] = '\0';

        if (line[0] == '\0') {
            continue;
        }

        activity_row_t row;

        if (!parse_activity_line(line, &row)) {
            fprintf(stderr, "WARNING: Skipping malformed line %ld\n", line_number);
            continue;
        }

        freq_summary_t *summary = find_or_create_summary(
            &summaries,
            &summary_count,
            &summary_capacity,
            row.target_hz,
            cfg.merge_hz
        );

        if (!summary) {
            fprintf(stderr, "ERROR: Out of memory creating summary\n");
            fclose(in);
            free(summaries);
            return 1;
        }

        update_summary(summary, &row);
        rows_read++;
    }

    fclose(in);

    if (summary_count == 0) {
        fprintf(stderr, "ERROR: No valid activity rows found\n");
        free(summaries);
        return 1;
    }

    if (cfg.sort_by_activity) {
        qsort(summaries, summary_count, sizeof(freq_summary_t), compare_summary_activity_desc);
    } else {
        qsort(summaries, summary_count, sizeof(freq_summary_t), compare_summary_freq);
    }

    FILE *out = fopen(cfg.output_file, "w");
    if (!out) {
        fprintf(stderr, "ERROR: Could not open output CSV: %s\n", cfg.output_file);
        free(summaries);
        return 1;
    }

    fprintf(out,
            "target_hz,target_mhz,total_checks,active_hits,percent_active,first_seen,last_seen,first_active,last_active,max_snr_db,min_snr_db,avg_snr_db,avg_active_snr_db,best_freq_hz,best_freq_mhz,best_power_db,best_noise_floor_db\n");

    printf("Activity summary:\n");
    printf("Rank  Target MHz    Checks  Hits   Active %%   Max SNR   Avg SNR   First Active        Last Active\n");
    printf("----  ----------    ------  ----   --------   -------   -------   ------------        -----------\n");

    int printed = 0;

    for (size_t i = 0; i < summary_count; i++) {
        freq_summary_t *s = &summaries[i];

        double pct = percent_active(s);

        if (s->active_hits < cfg.min_hits) {
            continue;
        }

        if (pct < cfg.min_percent_active) {
            continue;
        }

        double avg_snr = s->total_checks > 0
            ? s->sum_snr_db / (double)s->total_checks
            : 0.0;

        double avg_active_snr = s->active_hits > 0
            ? s->sum_active_snr_db / (double)s->active_hits
            : 0.0;

        const char *first_active = s->has_active ? s->first_active : "";
        const char *last_active = s->has_active ? s->last_active : "";

        fprintf(out,
                "%.3f,%.6f,%d,%d,%.2f,%s,%s,%s,%s,%.2f,%.2f,%.2f,%.2f,%.3f,%.6f,%.2f,%.2f\n",
                s->target_hz,
                s->target_hz / 1e6,
                s->total_checks,
                s->active_hits,
                pct,
                s->first_seen,
                s->last_seen,
                first_active,
                last_active,
                s->max_snr_db,
                s->min_snr_db,
                avg_snr,
                avg_active_snr,
                s->best_freq_hz,
                s->best_freq_hz / 1e6,
                s->best_power_db,
                s->best_noise_floor_db);

        printed++;

        printf("%4d  %10.6f    %6d  %4d   %8.2f   %7.2f   %7.2f   %-19s %-19s\n",
               printed,
               s->target_hz / 1e6,
               s->total_checks,
               s->active_hits,
               pct,
               s->max_snr_db,
               avg_snr,
               first_active,
               last_active);
    }

    fclose(out);

    printf("\n");
    printf("Rows read:             %ld\n", rows_read);
    printf("Frequencies summarized:%zu\n", summary_count);
    printf("Rows printed:          %d\n", printed);
    printf("Summary CSV:           %s\n", cfg.output_file);
    printf("\n");

    free(summaries);
    return 0;
}
