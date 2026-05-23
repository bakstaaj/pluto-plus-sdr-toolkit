#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    long long center_hz;
    double peak_hz;
    double offset_hz;
    double power_db;
    double noise_floor_db;
    double snr_db;
    int rank;
} detection_t;

typedef struct {
    double min_hz;
    double max_hz;
    double sum_hz;
    double sum_snr_db;
    double sum_noise_floor_db;

    double best_peak_hz;
    double best_power_db;
    double best_snr_db;
    double best_center_hz;

    int detections;
} group_t;

typedef struct {
    const char *input_file;
    const char *output_file;

    double merge_hz;
    double min_snr_db;
    int min_count;

    bool sort_by_snr;
} app_config_t;

static void print_usage(const char *prog)
{
    printf("\n");
    printf("Pluto+ Scan Group Tool\n");
    printf("\n");
    printf("Usage:\n");
    printf("  %s --in <sweep.csv> [options]\n", prog);
    printf("\n");
    printf("Options:\n");
    printf("  --in <file>         Input CSV from pluto_sweep_scanner\n");
    printf("  --out <file>        Output grouped CSV, default grouped_signals.csv\n");
    printf("  --merge-hz <hz>     Merge detections within this spacing, default 12500\n");
    printf("  --min-snr <db>      Ignore detections below this SNR, default 0\n");
    printf("  --min-count <n>     Only output groups with at least n detections, default 1\n");
    printf("  --sort <mode>       freq or snr, default freq\n");
    printf("  --help              Show this help\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s --in two_meter_scan.csv --out two_meter_grouped.csv\n", prog);
    printf("  %s --in airband_scan.csv --merge-hz 25000 --min-snr 10 --out airband_grouped.csv\n", prog);
    printf("  %s --in uhf_70cm_scan.csv --sort snr --min-count 2\n", prog);
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
    cfg->output_file = "grouped_signals.csv";
    cfg->merge_hz = 12500.0;
    cfg->min_snr_db = 0.0;
    cfg->min_count = 1;
    cfg->sort_by_snr = false;

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
        } else if (strcmp(arg, "--min-snr") == 0) {
            if (++i >= argc || !parse_double_value(argv[i], &cfg->min_snr_db)) {
                fprintf(stderr, "ERROR: --min-snr requires a numeric value\n");
                return false;
            }
        } else if (strcmp(arg, "--min-count") == 0) {
            if (++i >= argc || !parse_int_value(argv[i], &cfg->min_count)) {
                fprintf(stderr, "ERROR: --min-count requires an integer value\n");
                return false;
            }
        } else if (strcmp(arg, "--sort") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "ERROR: --sort requires freq or snr\n");
                return false;
            }

            if (strcmp(argv[i], "freq") == 0) {
                cfg->sort_by_snr = false;
            } else if (strcmp(argv[i], "snr") == 0) {
                cfg->sort_by_snr = true;
            } else {
                fprintf(stderr, "ERROR: --sort must be freq or snr\n");
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

    if (cfg->merge_hz <= 0.0) {
        fprintf(stderr, "ERROR: --merge-hz must be greater than zero\n");
        return false;
    }

    if (cfg->min_count <= 0) {
        fprintf(stderr, "ERROR: --min-count must be greater than zero\n");
        return false;
    }

    return true;
}

static int compare_detection_freq(const void *a, const void *b)
{
    const detection_t *da = (const detection_t *)a;
    const detection_t *db = (const detection_t *)b;

    if (da->peak_hz < db->peak_hz) {
        return -1;
    }

    if (da->peak_hz > db->peak_hz) {
        return 1;
    }

    return 0;
}

static int compare_group_freq(const void *a, const void *b)
{
    const group_t *ga = (const group_t *)a;
    const group_t *gb = (const group_t *)b;

    double ca = ga->sum_hz / ga->detections;
    double cb = gb->sum_hz / gb->detections;

    if (ca < cb) {
        return -1;
    }

    if (ca > cb) {
        return 1;
    }

    return 0;
}

static int compare_group_snr_desc(const void *a, const void *b)
{
    const group_t *ga = (const group_t *)a;
    const group_t *gb = (const group_t *)b;

    if (ga->best_snr_db > gb->best_snr_db) {
        return -1;
    }

    if (ga->best_snr_db < gb->best_snr_db) {
        return 1;
    }

    return 0;
}

static void add_detection_to_group(group_t *g, const detection_t *d)
{
    if (g->detections == 0) {
        g->min_hz = d->peak_hz;
        g->max_hz = d->peak_hz;
        g->sum_hz = d->peak_hz;
        g->sum_snr_db = d->snr_db;
        g->sum_noise_floor_db = d->noise_floor_db;

        g->best_peak_hz = d->peak_hz;
        g->best_power_db = d->power_db;
        g->best_snr_db = d->snr_db;
        g->best_center_hz = (double)d->center_hz;

        g->detections = 1;
        return;
    }

    if (d->peak_hz < g->min_hz) {
        g->min_hz = d->peak_hz;
    }

    if (d->peak_hz > g->max_hz) {
        g->max_hz = d->peak_hz;
    }

    g->sum_hz += d->peak_hz;
    g->sum_snr_db += d->snr_db;
    g->sum_noise_floor_db += d->noise_floor_db;

    if (d->snr_db > g->best_snr_db) {
        g->best_peak_hz = d->peak_hz;
        g->best_power_db = d->power_db;
        g->best_snr_db = d->snr_db;
        g->best_center_hz = (double)d->center_hz;
    }

    g->detections++;
}

static bool parse_detection_line(char *line, detection_t *d)
{
    /*
       Expected input:
       center_hz,peak_hz,offset_hz,power_db,noise_floor_db,snr_db,rank
    */

    char *fields[7] = {0};
    int count = 0;

    char *token = strtok(line, ",");

    while (token && count < 7) {
        fields[count++] = token;
        token = strtok(NULL, ",");
    }

    if (count != 7) {
        return false;
    }

    char *end = NULL;

    errno = 0;
    d->center_hz = strtoll(fields[0], &end, 10);
    if (errno != 0 || end == fields[0]) {
        return false;
    }

    errno = 0;
    d->peak_hz = strtod(fields[1], &end);
    if (errno != 0 || end == fields[1]) {
        return false;
    }

    errno = 0;
    d->offset_hz = strtod(fields[2], &end);
    if (errno != 0 || end == fields[2]) {
        return false;
    }

    errno = 0;
    d->power_db = strtod(fields[3], &end);
    if (errno != 0 || end == fields[3]) {
        return false;
    }

    errno = 0;
    d->noise_floor_db = strtod(fields[4], &end);
    if (errno != 0 || end == fields[4]) {
        return false;
    }

    errno = 0;
    d->snr_db = strtod(fields[5], &end);
    if (errno != 0 || end == fields[5]) {
        return false;
    }

    errno = 0;
    d->rank = (int)strtol(fields[6], &end, 10);
    if (errno != 0 || end == fields[6]) {
        return false;
    }

    return true;
}

static bool append_detection(detection_t **items, size_t *count, size_t *capacity, const detection_t *d)
{
    if (*count >= *capacity) {
        size_t new_capacity = (*capacity == 0) ? 256 : (*capacity * 2);

        detection_t *new_items = (detection_t *)realloc(*items, new_capacity * sizeof(detection_t));
        if (!new_items) {
            return false;
        }

        *items = new_items;
        *capacity = new_capacity;
    }

    (*items)[*count] = *d;
    (*count)++;

    return true;
}

static bool append_group(group_t **items, size_t *count, size_t *capacity, const group_t *g)
{
    if (*count >= *capacity) {
        size_t new_capacity = (*capacity == 0) ? 128 : (*capacity * 2);

        group_t *new_items = (group_t *)realloc(*items, new_capacity * sizeof(group_t));
        if (!new_items) {
            return false;
        }

        *items = new_items;
        *capacity = new_capacity;
    }

    (*items)[*count] = *g;
    (*count)++;

    return true;
}

int main(int argc, char **argv)
{
    app_config_t cfg;

    if (!parse_args(argc, argv, &cfg)) {
        print_usage(argv[0]);
        return 1;
    }

    printf("\n");
    printf("Pluto+ Scan Group Tool\n");
    printf("----------------------\n");
    printf("Input CSV:       %s\n", cfg.input_file);
    printf("Output CSV:      %s\n", cfg.output_file);
    printf("Merge spacing:   %.1f Hz\n", cfg.merge_hz);
    printf("Minimum SNR:     %.2f dB\n", cfg.min_snr_db);
    printf("Minimum count:   %d\n", cfg.min_count);
    printf("Sort:            %s\n", cfg.sort_by_snr ? "snr" : "freq");
    printf("\n");

    FILE *in = fopen(cfg.input_file, "r");
    if (!in) {
        fprintf(stderr, "ERROR: Could not open input file: %s\n", cfg.input_file);
        return 1;
    }

    detection_t *detections = NULL;
    size_t detection_count = 0;
    size_t detection_capacity = 0;

    char line[1024];
    long line_number = 0;

    while (fgets(line, sizeof(line), in)) {
        line_number++;

        /*
           Skip header.
        */
        if (line_number == 1 && strstr(line, "center_hz") != NULL) {
            continue;
        }

        /*
           Remove trailing newline.
        */
        line[strcspn(line, "\r\n")] = '\0';

        if (line[0] == '\0') {
            continue;
        }

        detection_t d;

        if (!parse_detection_line(line, &d)) {
            fprintf(stderr, "WARNING: Skipping malformed CSV line %ld\n", line_number);
            continue;
        }

        if (d.snr_db < cfg.min_snr_db) {
            continue;
        }

        if (!append_detection(&detections, &detection_count, &detection_capacity, &d)) {
            fprintf(stderr, "ERROR: Out of memory while reading detections\n");
            fclose(in);
            free(detections);
            return 1;
        }
    }

    fclose(in);

    if (detection_count == 0) {
        fprintf(stderr, "No detections matched the filters.\n");
        free(detections);
        return 1;
    }

    qsort(detections, detection_count, sizeof(detection_t), compare_detection_freq);

    group_t *groups = NULL;
    size_t group_count = 0;
    size_t group_capacity = 0;

    group_t current;
    memset(&current, 0, sizeof(current));

    for (size_t i = 0; i < detection_count; i++) {
        detection_t *d = &detections[i];

        if (current.detections == 0) {
            add_detection_to_group(&current, d);
            continue;
        }

        /*
           Merge if this detection is close to the existing group's max frequency.
           This handles clusters that spread over multiple FFT bins or nearby sweep steps.
        */
        if (d->peak_hz <= current.max_hz + cfg.merge_hz) {
            add_detection_to_group(&current, d);
        } else {
            if (current.detections >= cfg.min_count) {
                if (!append_group(&groups, &group_count, &group_capacity, &current)) {
                    fprintf(stderr, "ERROR: Out of memory while creating groups\n");
                    free(detections);
                    free(groups);
                    return 1;
                }
            }

            memset(&current, 0, sizeof(current));
            add_detection_to_group(&current, d);
        }
    }

    if (current.detections >= cfg.min_count) {
        if (!append_group(&groups, &group_count, &group_capacity, &current)) {
            fprintf(stderr, "ERROR: Out of memory while creating final group\n");
            free(detections);
            free(groups);
            return 1;
        }
    }

    if (group_count == 0) {
        fprintf(stderr, "No groups matched the filters.\n");
        free(detections);
        free(groups);
        return 1;
    }

    if (cfg.sort_by_snr) {
        qsort(groups, group_count, sizeof(group_t), compare_group_snr_desc);
    } else {
        qsort(groups, group_count, sizeof(group_t), compare_group_freq);
    }

    FILE *out = fopen(cfg.output_file, "w");
    if (!out) {
        fprintf(stderr, "ERROR: Could not open output file: %s\n", cfg.output_file);
        free(detections);
        free(groups);
        return 1;
    }

    fprintf(out,
            "group_center_hz,best_peak_hz,min_hz,max_hz,width_hz,detections,best_snr_db,avg_snr_db,best_power_db,avg_noise_floor_db,best_tune_center_hz\n");

    printf("Grouped signals:\n");
    printf("Rank  Center MHz      Best MHz        Width Hz    Hits   Best SNR   Avg SNR\n");
    printf("----  ----------      --------        --------    ----   --------   -------\n");

    for (size_t i = 0; i < group_count; i++) {
        group_t *g = &groups[i];

        double group_center_hz = g->sum_hz / (double)g->detections;
        double width_hz = g->max_hz - g->min_hz;
        double avg_snr_db = g->sum_snr_db / (double)g->detections;
        double avg_noise_floor_db = g->sum_noise_floor_db / (double)g->detections;

        fprintf(out,
                "%.3f,%.3f,%.3f,%.3f,%.3f,%d,%.2f,%.2f,%.2f,%.2f,%.3f\n",
                group_center_hz,
                g->best_peak_hz,
                g->min_hz,
                g->max_hz,
                width_hz,
                g->detections,
                g->best_snr_db,
                avg_snr_db,
                g->best_power_db,
                avg_noise_floor_db,
                g->best_center_hz);

        printf("%4zu  %10.6f      %10.6f      %8.1f    %4d   %8.2f   %7.2f\n",
               i + 1,
               group_center_hz / 1e6,
               g->best_peak_hz / 1e6,
               width_hz,
               g->detections,
               g->best_snr_db,
               avg_snr_db);
    }

    fclose(out);

    printf("\n");
    printf("Input detections:  %zu\n", detection_count);
    printf("Output groups:     %zu\n", group_count);
    printf("Grouped CSV:       %s\n", cfg.output_file);
    printf("\n");

    free(detections);
    free(groups);

    return 0;
}
