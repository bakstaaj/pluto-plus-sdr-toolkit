#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
   pluto_scan_session.c

   High-level Pluto+ SDR scan workflow runner.

   Runs:
     1. pluto_band_scan.exe
     2. pluto_activity_monitor.exe
     3. pluto_activity_summary.exe
     4. pluto_session_report.exe

   Supports simple key=value config files via:
     --config configs/2m.conf

   Command-line options override config-file values.
*/

typedef struct {
    const char *band;
    const char *uri;
    const char *tool_dir;
    const char *config_file;
    const char *out_prefix;
    const char *gain_mode;
    const char *summary_sort;
    const char *report_out;
    const char *report_title;

    long long gain_db;

    int cycles;
    int scan_avg;
    int activity_avg;
    int min_active_hits;
    int report_max_rows;

    double scan_threshold_db;
    double activity_threshold_db;
    double passband_hz;

    bool use_gain_db;
    bool has_scan_avg;
    bool has_activity_avg;
    bool has_scan_threshold;
    bool has_activity_threshold;
    bool has_passband;
    bool skip_scan;
    bool skip_activity;
    bool generate_report;
    bool dry_run;
} app_config_t;

static char *dup_string(const char *s)
{
    if (!s) {
        return NULL;
    }

    size_t len = strlen(s);
    char *copy = (char *)malloc(len + 1);

    if (!copy) {
        return NULL;
    }

    memcpy(copy, s, len + 1);
    return copy;
}

static bool set_string(const char **dest, const char *value)
{
    char *copy = dup_string(value);

    if (!copy) {
        fprintf(stderr, "ERROR: out of memory copying string value\n");
        return false;
    }

    *dest = copy;
    return true;
}

static void print_usage(const char *prog)
{
    printf("\n");
    printf("Pluto+ Scan Session Runner\n");
    printf("\n");
    printf("Runs the complete workflow:\n");
    printf("  1. pluto_band_scan.exe\n");
    printf("  2. pluto_activity_monitor.exe\n");
    printf("  3. pluto_activity_summary.exe\n");
    printf("  4. pluto_session_report.exe\n");
    printf("\n");
    printf("Usage:\n");
    printf("  %s --band <preset> [options]\n", prog);
    printf("  %s --config <file> [options]\n", prog);
    printf("\n");
    printf("Common band presets from pluto_band_scan:\n");
    printf("  2m\n");
    printf("  airband\n");
    printf("  70cm\n");
    printf("  noaa\n");
    printf("  fm\n");
    printf("  ism915\n");
    printf("\n");
    printf("Options:\n");
    printf("  --config <file>                Load session options from config file\n");
    printf("  --band <name>                  Band preset to scan\n");
    printf("  --uri <uri>                    IIO URI, default ip:192.168.2.1\n");
    printf("  --tool-dir <dir>               Directory containing the built EXEs\n");
    printf("  --out-prefix <prefix>          Output prefix, default is band name\n");
    printf("  --cycles <count>               Activity monitor cycles, default 10\n");
    printf("  --scan-threshold-db <db>       Threshold for band scan/grouping\n");
    printf("  --activity-threshold-db <db>   Threshold for activity monitor, default 10\n");
    printf("  --threshold-db <db>            Set both scan and activity thresholds\n");
    printf("  --scan-avg <count>             FFT averages for band scan\n");
    printf("  --activity-avg <count>         FFT averages for activity monitor, default 4\n");
    printf("  --passband-hz <hz>             Activity passband width\n");
    printf("  --gain-mode <mode>             slow_attack, fast_attack, manual\n");
    printf("  --gain-db <db>                 Manual gain dB, implies manual gain mode\n");
    printf("  --summary-sort <mode>          freq or activity. Default activity\n");
    printf("  --min-active-hits <count>      Summary filter, default 0\n");
    printf("  --skip-scan                    Reuse existing <prefix>_grouped.csv\n");
    printf("  --skip-activity                Reuse existing <prefix>_activity.csv\n");
    printf("  --report                       Generate HTML report, default enabled\n");
    printf("  --no-report                    Do not generate HTML report\n");
    printf("  --report-out <file>            Report HTML file, default <prefix>_report.html\n");
    printf("  --report-title <text>          Report title\n");
    printf("  --report-max-rows <count>      Max rows per report table, default 200\n");
    printf("  --dry-run                      Print commands but do not execute\n");
    printf("  --help                         Show this help\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s --band 2m --cycles 20\n", prog);
    printf("  %s --config configs/2m.conf\n", prog);
    printf("  %s --config configs/airband.conf --cycles 30\n", prog);
    printf("  %s --band airband --cycles 10 --passband-hz 25000\n", prog);
    printf("  %s --band noaa --cycles 10 --activity-threshold-db 8\n", prog);
    printf("  %s --band 70cm --gain-db 40 --cycles 20 --summary-sort activity\n", prog);
    printf("  %s --band 2m --skip-scan --cycles 30\n", prog);
    printf("\n");
}

static bool parse_ll(const char *text, long long *value)
{
    char *end = NULL;
    errno = 0;

    long long v = strtoll(text, &end, 10);

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

static double default_passband_for_band(const char *band)
{
    if (!band) {
        return 12500.0;
    }

    if (strcmp(band, "airband") == 0) {
        return 25000.0;
    }

    if (strcmp(band, "noaa") == 0) {
        return 25000.0;
    }

    if (strcmp(band, "fm") == 0) {
        return 200000.0;
    }

    if (strcmp(band, "ism915") == 0) {
        return 50000.0;
    }

    return 12500.0;
}

static void trim_whitespace(char *s)
{
    char *start = s;

    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n') {
        start++;
    }

    if (start != s) {
        memmove(s, start, strlen(start) + 1);
    }

    size_t len = strlen(s);

    while (len > 0 &&
           (s[len - 1] == ' ' ||
            s[len - 1] == '\t' ||
            s[len - 1] == '\r' ||
            s[len - 1] == '\n')) {
        s[len - 1] = '\0';
        len--;
    }
}

static void unquote_value(char *s)
{
    size_t len = strlen(s);

    if (len >= 2) {
        if ((s[0] == '"' && s[len - 1] == '"') ||
            (s[0] == '\'' && s[len - 1] == '\'')) {
            memmove(s, s + 1, len - 2);
            s[len - 2] = '\0';
        }
    }
}

static bool parse_bool_value(const char *text, bool *value)
{
    if (strcmp(text, "1") == 0 ||
        strcmp(text, "true") == 0 ||
        strcmp(text, "yes") == 0 ||
        strcmp(text, "on") == 0) {
        *value = true;
        return true;
    }

    if (strcmp(text, "0") == 0 ||
        strcmp(text, "false") == 0 ||
        strcmp(text, "no") == 0 ||
        strcmp(text, "off") == 0) {
        *value = false;
        return true;
    }

    return false;
}

static bool apply_config_value(app_config_t *cfg, const char *key, const char *value)
{
    if (strcmp(key, "band") == 0) {
        return set_string(&cfg->band, value);
    } else if (strcmp(key, "uri") == 0) {
        return set_string(&cfg->uri, value);
    } else if (strcmp(key, "tool_dir") == 0) {
        return set_string(&cfg->tool_dir, value);
    } else if (strcmp(key, "out_prefix") == 0) {
        return set_string(&cfg->out_prefix, value);
    } else if (strcmp(key, "cycles") == 0) {
        if (!parse_int_value(value, &cfg->cycles)) {
            fprintf(stderr, "ERROR: invalid config value for cycles: %s\n", value);
            return false;
        }
    } else if (strcmp(key, "scan_threshold_db") == 0) {
        if (!parse_double_value(value, &cfg->scan_threshold_db)) {
            fprintf(stderr, "ERROR: invalid config value for scan_threshold_db: %s\n", value);
            return false;
        }
        cfg->has_scan_threshold = true;
    } else if (strcmp(key, "activity_threshold_db") == 0) {
        if (!parse_double_value(value, &cfg->activity_threshold_db)) {
            fprintf(stderr, "ERROR: invalid config value for activity_threshold_db: %s\n", value);
            return false;
        }
        cfg->has_activity_threshold = true;
    } else if (strcmp(key, "threshold_db") == 0) {
        double v = 0.0;
        if (!parse_double_value(value, &v)) {
            fprintf(stderr, "ERROR: invalid config value for threshold_db: %s\n", value);
            return false;
        }
        cfg->scan_threshold_db = v;
        cfg->activity_threshold_db = v;
        cfg->has_scan_threshold = true;
        cfg->has_activity_threshold = true;
    } else if (strcmp(key, "scan_avg") == 0) {
        if (!parse_int_value(value, &cfg->scan_avg)) {
            fprintf(stderr, "ERROR: invalid config value for scan_avg: %s\n", value);
            return false;
        }
        cfg->has_scan_avg = true;
    } else if (strcmp(key, "activity_avg") == 0) {
        if (!parse_int_value(value, &cfg->activity_avg)) {
            fprintf(stderr, "ERROR: invalid config value for activity_avg: %s\n", value);
            return false;
        }
        cfg->has_activity_avg = true;
    } else if (strcmp(key, "passband_hz") == 0) {
        if (!parse_double_value(value, &cfg->passband_hz)) {
            fprintf(stderr, "ERROR: invalid config value for passband_hz: %s\n", value);
            return false;
        }
        cfg->has_passband = true;
    } else if (strcmp(key, "gain_mode") == 0) {
        return set_string(&cfg->gain_mode, value);
    } else if (strcmp(key, "gain_db") == 0) {
        if (!parse_ll(value, &cfg->gain_db)) {
            fprintf(stderr, "ERROR: invalid config value for gain_db: %s\n", value);
            return false;
        }
        cfg->gain_mode = "manual";
        cfg->use_gain_db = true;
    } else if (strcmp(key, "summary_sort") == 0) {
        if (strcmp(value, "freq") != 0 && strcmp(value, "activity") != 0) {
            fprintf(stderr, "ERROR: summary_sort must be freq or activity\n");
            return false;
        }
        return set_string(&cfg->summary_sort, value);
    } else if (strcmp(key, "min_active_hits") == 0 || strcmp(key, "min_hits") == 0) {
        if (!parse_int_value(value, &cfg->min_active_hits)) {
            fprintf(stderr, "ERROR: invalid config value for min_active_hits: %s\n", value);
            return false;
        }
    } else if (strcmp(key, "skip_scan") == 0) {
        if (!parse_bool_value(value, &cfg->skip_scan)) {
            fprintf(stderr, "ERROR: invalid config value for skip_scan: %s\n", value);
            return false;
        }
    } else if (strcmp(key, "skip_activity") == 0) {
        if (!parse_bool_value(value, &cfg->skip_activity)) {
            fprintf(stderr, "ERROR: invalid config value for skip_activity: %s\n", value);
            return false;
        }
    } else if (strcmp(key, "report") == 0 || strcmp(key, "generate_report") == 0) {
        if (!parse_bool_value(value, &cfg->generate_report)) {
            fprintf(stderr, "ERROR: invalid config value for report: %s\n", value);
            return false;
        }
    } else if (strcmp(key, "report_out") == 0) {
        return set_string(&cfg->report_out, value);
    } else if (strcmp(key, "report_title") == 0) {
        return set_string(&cfg->report_title, value);
    } else if (strcmp(key, "report_max_rows") == 0) {
        if (!parse_int_value(value, &cfg->report_max_rows)) {
            fprintf(stderr, "ERROR: invalid config value for report_max_rows: %s\n", value);
            return false;
        }
    } else if (strcmp(key, "dry_run") == 0) {
        if (!parse_bool_value(value, &cfg->dry_run)) {
            fprintf(stderr, "ERROR: invalid config value for dry_run: %s\n", value);
            return false;
        }
    } else {
        fprintf(stderr, "WARNING: unknown config key ignored: %s\n", key);
    }

    return true;
}

static bool load_config_file(app_config_t *cfg, const char *filename)
{
    FILE *f = fopen(filename, "r");

    if (!f) {
        fprintf(stderr, "ERROR: could not open config file: %s\n", filename);
        return false;
    }

    char line[2048];
    long line_number = 0;

    while (fgets(line, sizeof(line), f)) {
        line_number++;

        line[strcspn(line, "\r\n")] = '\0';

        char *comment = strchr(line, '#');
        if (comment) {
            *comment = '\0';
        }

        comment = strchr(line, ';');
        if (comment) {
            *comment = '\0';
        }

        trim_whitespace(line);

        if (line[0] == '\0') {
            continue;
        }

        char *equals = strchr(line, '=');

        if (!equals) {
            fprintf(stderr, "WARNING: skipping malformed config line %ld\n", line_number);
            continue;
        }

        *equals = '\0';

        char *key = line;
        char *value = equals + 1;

        trim_whitespace(key);
        trim_whitespace(value);
        unquote_value(value);

        if (key[0] == '\0') {
            fprintf(stderr, "WARNING: skipping empty config key on line %ld\n", line_number);
            continue;
        }

        if (!apply_config_value(cfg, key, value)) {
            fclose(f);
            return false;
        }
    }

    fclose(f);
    return true;
}

static bool parse_args(int argc, char **argv, app_config_t *cfg)
{
    cfg->band = NULL;
    cfg->uri = "ip:192.168.2.1";
    cfg->tool_dir = NULL;
    cfg->config_file = NULL;
    cfg->out_prefix = NULL;
    cfg->gain_mode = "slow_attack";
    cfg->summary_sort = "activity";
    cfg->report_out = NULL;
    cfg->report_title = "Pluto+ SDR Scan Session Report";

    cfg->gain_db = 30;

    cfg->cycles = 10;
    cfg->scan_avg = 0;
    cfg->activity_avg = 4;
    cfg->min_active_hits = 0;
    cfg->report_max_rows = 200;

    cfg->scan_threshold_db = 0.0;
    cfg->activity_threshold_db = 10.0;
    cfg->passband_hz = 0.0;

    cfg->use_gain_db = false;
    cfg->has_scan_avg = false;
    cfg->has_activity_avg = false;
    cfg->has_scan_threshold = false;
    cfg->has_activity_threshold = false;
    cfg->has_passband = false;
    cfg->skip_scan = false;
    cfg->skip_activity = false;
    cfg->generate_report = true;
    cfg->dry_run = false;

    /*
       First pass: load config file before normal command-line parsing.
       Later command-line arguments override config-file values.
    */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--config") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "ERROR: --config requires a filename\n");
                return false;
            }

            cfg->config_file = argv[i];

            if (!load_config_file(cfg, cfg->config_file)) {
                return false;
            }
        }
    }

    /*
       Second pass: normal CLI parsing. These options override config values.
    */
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];

        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            print_usage(argv[0]);
            exit(0);
        } else if (strcmp(arg, "--config") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "ERROR: --config requires a filename\n");
                return false;
            }
            cfg->config_file = argv[i];
        } else if (strcmp(arg, "--band") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "ERROR: --band requires a value\n");
                return false;
            }
            cfg->band = argv[i];
        } else if (strcmp(arg, "--uri") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "ERROR: --uri requires a value\n");
                return false;
            }
            cfg->uri = argv[i];
        } else if (strcmp(arg, "--tool-dir") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "ERROR: --tool-dir requires a directory\n");
                return false;
            }
            cfg->tool_dir = argv[i];
        } else if (strcmp(arg, "--out-prefix") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "ERROR: --out-prefix requires a value\n");
                return false;
            }
            cfg->out_prefix = argv[i];
        } else if (strcmp(arg, "--cycles") == 0) {
            if (++i >= argc || !parse_int_value(argv[i], &cfg->cycles)) {
                fprintf(stderr, "ERROR: --cycles requires an integer value\n");
                return false;
            }
        } else if (strcmp(arg, "--scan-threshold-db") == 0) {
            if (++i >= argc || !parse_double_value(argv[i], &cfg->scan_threshold_db)) {
                fprintf(stderr, "ERROR: --scan-threshold-db requires a numeric value\n");
                return false;
            }
            cfg->has_scan_threshold = true;
        } else if (strcmp(arg, "--activity-threshold-db") == 0) {
            if (++i >= argc || !parse_double_value(argv[i], &cfg->activity_threshold_db)) {
                fprintf(stderr, "ERROR: --activity-threshold-db requires a numeric value\n");
                return false;
            }
            cfg->has_activity_threshold = true;
        } else if (strcmp(arg, "--threshold-db") == 0) {
            double v = 0.0;
            if (++i >= argc || !parse_double_value(argv[i], &v)) {
                fprintf(stderr, "ERROR: --threshold-db requires a numeric value\n");
                return false;
            }
            cfg->scan_threshold_db = v;
            cfg->activity_threshold_db = v;
            cfg->has_scan_threshold = true;
            cfg->has_activity_threshold = true;
        } else if (strcmp(arg, "--scan-avg") == 0) {
            if (++i >= argc || !parse_int_value(argv[i], &cfg->scan_avg)) {
                fprintf(stderr, "ERROR: --scan-avg requires an integer value\n");
                return false;
            }
            cfg->has_scan_avg = true;
        } else if (strcmp(arg, "--activity-avg") == 0) {
            if (++i >= argc || !parse_int_value(argv[i], &cfg->activity_avg)) {
                fprintf(stderr, "ERROR: --activity-avg requires an integer value\n");
                return false;
            }
            cfg->has_activity_avg = true;
        } else if (strcmp(arg, "--passband-hz") == 0) {
            if (++i >= argc || !parse_double_value(argv[i], &cfg->passband_hz)) {
                fprintf(stderr, "ERROR: --passband-hz requires a numeric value\n");
                return false;
            }
            cfg->has_passband = true;
        } else if (strcmp(arg, "--gain-mode") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "ERROR: --gain-mode requires a value\n");
                return false;
            }
            cfg->gain_mode = argv[i];
            cfg->use_gain_db = (strcmp(cfg->gain_mode, "manual") == 0);
        } else if (strcmp(arg, "--gain-db") == 0) {
            if (++i >= argc || !parse_ll(argv[i], &cfg->gain_db)) {
                fprintf(stderr, "ERROR: --gain-db requires an integer dB value\n");
                return false;
            }
            cfg->gain_mode = "manual";
            cfg->use_gain_db = true;
        } else if (strcmp(arg, "--summary-sort") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "ERROR: --summary-sort requires freq or activity\n");
                return false;
            }

            if (strcmp(argv[i], "freq") != 0 &&
                strcmp(argv[i], "activity") != 0) {
                fprintf(stderr, "ERROR: --summary-sort must be freq or activity\n");
                return false;
            }

            cfg->summary_sort = argv[i];
        } else if (strcmp(arg, "--min-active-hits") == 0 || strcmp(arg, "--min-hits") == 0) {
            if (++i >= argc || !parse_int_value(argv[i], &cfg->min_active_hits)) {
                fprintf(stderr, "ERROR: --min-active-hits requires an integer value\n");
                return false;
            }
        } else if (strcmp(arg, "--skip-scan") == 0) {
            cfg->skip_scan = true;
        } else if (strcmp(arg, "--skip-activity") == 0) {
            cfg->skip_activity = true;
        } else if (strcmp(arg, "--report") == 0) {
            cfg->generate_report = true;
        } else if (strcmp(arg, "--no-report") == 0) {
            cfg->generate_report = false;
        } else if (strcmp(arg, "--report-out") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "ERROR: --report-out requires a filename\n");
                return false;
            }
            cfg->report_out = argv[i];
        } else if (strcmp(arg, "--report-title") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "ERROR: --report-title requires text\n");
                return false;
            }
            cfg->report_title = argv[i];
        } else if (strcmp(arg, "--report-max-rows") == 0) {
            if (++i >= argc || !parse_int_value(argv[i], &cfg->report_max_rows)) {
                fprintf(stderr, "ERROR: --report-max-rows requires an integer value\n");
                return false;
            }
        } else if (strcmp(arg, "--dry-run") == 0) {
            cfg->dry_run = true;
        } else {
            fprintf(stderr, "ERROR: Unknown option: %s\n", arg);
            return false;
        }
    }

    if (!cfg->band) {
        fprintf(stderr, "ERROR: --band is required, either directly or through --config\n");
        return false;
    }

    if (cfg->cycles <= 0) {
        fprintf(stderr, "ERROR: --cycles must be greater than zero\n");
        return false;
    }

    if (cfg->has_scan_avg && cfg->scan_avg <= 0) {
        fprintf(stderr, "ERROR: --scan-avg must be greater than zero\n");
        return false;
    }

    if (cfg->activity_avg <= 0) {
        fprintf(stderr, "ERROR: --activity-avg must be greater than zero\n");
        return false;
    }

    if (cfg->min_active_hits < 0) {
        fprintf(stderr, "ERROR: --min-active-hits must not be negative\n");
        return false;
    }

    if (cfg->report_max_rows <= 0) {
        fprintf(stderr, "ERROR: --report-max-rows must be greater than zero\n");
        return false;
    }

    if (!cfg->has_passband) {
        cfg->passband_hz = default_passband_for_band(cfg->band);
    }

    if (strcmp(cfg->gain_mode, "manual") == 0) {
        cfg->use_gain_db = true;
    }

    return true;
}

static void get_default_tool_dir(const char *argv0, char *out, size_t out_size)
{
    const char *last_slash = strrchr(argv0, '/');
    const char *last_backslash = strrchr(argv0, '\\');
    const char *last = last_slash;

    if (last_backslash && (!last || last_backslash > last)) {
        last = last_backslash;
    }

    if (!last) {
        snprintf(out, out_size, ".");
        return;
    }

    size_t len = (size_t)(last - argv0);

    if (len == 0 || len >= out_size) {
        snprintf(out, out_size, ".");
        return;
    }

    memcpy(out, argv0, len);
    out[len] = '\0';
}

static void normalize_path_separators(char *s)
{
    for (; *s; s++) {
        if (*s == '/') {
            *s = '\\';
        }
    }
}

static bool appendf(char *buf, size_t buf_size, const char *fmt, ...)
{
    size_t len = strlen(buf);

    if (len >= buf_size) {
        return false;
    }

    va_list ap;
    va_start(ap, fmt);

    int ret = vsnprintf(buf + len, buf_size - len, fmt, ap);

    va_end(ap);

    if (ret < 0) {
        return false;
    }

    if ((size_t)ret >= buf_size - len) {
        return false;
    }

    return true;
}

static int run_command(const char *label, const char *command, bool dry_run)
{
    printf("\n%s command:\n%s\n", label, command);

    if (dry_run) {
        printf("Dry run enabled, not executing.\n");
        return 0;
    }

    printf("\nRunning %s...\n", label);

    int ret = system(command);

    if (ret != 0) {
        fprintf(stderr, "ERROR: %s failed with return code %d\n", label, ret);
        return ret;
    }

    return 0;
}

int main(int argc, char **argv)
{
    app_config_t cfg;

    if (!parse_args(argc, argv, &cfg)) {
        print_usage(argv[0]);
        return 1;
    }

    char derived_tool_dir[512];
    get_default_tool_dir(argv[0], derived_tool_dir, sizeof(derived_tool_dir));

    char tool_dir[512];

    if (cfg.tool_dir) {
        snprintf(tool_dir, sizeof(tool_dir), "%s", cfg.tool_dir);
    } else {
        snprintf(tool_dir, sizeof(tool_dir), "%s", derived_tool_dir);
    }

    normalize_path_separators(tool_dir);

    const char *prefix = cfg.out_prefix ? cfg.out_prefix : cfg.band;

    char raw_csv[512];
    char grouped_csv[512];
    char activity_csv[512];
    char summary_csv[512];
    char report_html[512];

    snprintf(raw_csv, sizeof(raw_csv), "%s_raw.csv", prefix);
    snprintf(grouped_csv, sizeof(grouped_csv), "%s_grouped.csv", prefix);
    snprintf(activity_csv, sizeof(activity_csv), "%s_activity.csv", prefix);
    snprintf(summary_csv, sizeof(summary_csv), "%s_summary.csv", prefix);
    snprintf(report_html, sizeof(report_html), "%s_report.html", prefix);

    const char *report_file = cfg.report_out ? cfg.report_out : report_html;

    printf("\n");
    printf("Pluto+ Scan Session Runner\n");
    printf("--------------------------\n");
    printf("Band:                  %s\n", cfg.band);
    printf("Config file:           %s\n", cfg.config_file ? cfg.config_file : "(none)");
    printf("URI:                   %s\n", cfg.uri);
    printf("Tool directory:        %s\n", tool_dir);
    printf("Output prefix:         %s\n", prefix);
    printf("Raw CSV:               %s\n", raw_csv);
    printf("Grouped CSV:           %s\n", grouped_csv);
    printf("Activity CSV:          %s\n", activity_csv);
    printf("Summary CSV:           %s\n", summary_csv);
    printf("Report HTML:           %s\n", report_file);
    printf("Activity cycles:       %d\n", cfg.cycles);
    printf("Activity threshold:    %.2f dB\n", cfg.activity_threshold_db);
    printf("Activity passband:     %.1f Hz\n", cfg.passband_hz);
    printf("Activity averages:     %d\n", cfg.activity_avg);
    printf("Gain mode:             %s\n", cfg.gain_mode);

    if (cfg.use_gain_db) {
        printf("Manual gain:           %lld dB\n", cfg.gain_db);
    }

    printf("Summary sort:          %s\n", cfg.summary_sort);
    printf("Min active hits:       %d\n", cfg.min_active_hits);
    printf("Skip scan:             %s\n", cfg.skip_scan ? "yes" : "no");
    printf("Skip activity:         %s\n", cfg.skip_activity ? "yes" : "no");
    printf("Generate report:       %s\n", cfg.generate_report ? "yes" : "no");
    printf("\n");

    if (!cfg.skip_scan) {
        char cmd[8192];
        cmd[0] = '\0';

        if (!appendf(cmd, sizeof(cmd),
                     "call \"%s\\pluto_band_scan.exe\" "
                     "--band \"%s\" "
                     "--uri \"%s\" "
                     "--out-prefix \"%s\" ",
                     tool_dir,
                     cfg.band,
                     cfg.uri,
                     prefix)) {
            fprintf(stderr, "ERROR: command buffer overflow\n");
            return 1;
        }

        if (cfg.has_scan_threshold) {
            if (!appendf(cmd, sizeof(cmd), "--threshold-db %.3f ", cfg.scan_threshold_db)) {
                fprintf(stderr, "ERROR: command buffer overflow\n");
                return 1;
            }
        }

        if (cfg.has_scan_avg) {
            if (!appendf(cmd, sizeof(cmd), "--avg %d ", cfg.scan_avg)) {
                fprintf(stderr, "ERROR: command buffer overflow\n");
                return 1;
            }
        }

        if (cfg.use_gain_db) {
            if (!appendf(cmd, sizeof(cmd), "--gain-db %lld ", cfg.gain_db)) {
                fprintf(stderr, "ERROR: command buffer overflow\n");
                return 1;
            }
        } else {
            if (!appendf(cmd, sizeof(cmd), "--gain-mode \"%s\" ", cfg.gain_mode)) {
                fprintf(stderr, "ERROR: command buffer overflow\n");
                return 1;
            }
        }

        int ret = run_command("Band scan", cmd, cfg.dry_run);
        if (ret != 0) {
            return 1;
        }
    } else {
        printf("Skipping band scan. Expected grouped CSV: %s\n", grouped_csv);
    }

    if (!cfg.skip_activity) {
        char cmd[8192];
        cmd[0] = '\0';

        if (!appendf(cmd, sizeof(cmd),
                     "call \"%s\\pluto_activity_monitor.exe\" "
                     "--uri \"%s\" "
                     "--in \"%s\" "
                     "--cycles %d "
                     "--threshold-db %.3f "
                     "--passband-hz %.3f "
                     "--avg %d "
                     "--csv \"%s\" ",
                     tool_dir,
                     cfg.uri,
                     grouped_csv,
                     cfg.cycles,
                     cfg.activity_threshold_db,
                     cfg.passband_hz,
                     cfg.activity_avg,
                     activity_csv)) {
            fprintf(stderr, "ERROR: command buffer overflow\n");
            return 1;
        }

        if (cfg.use_gain_db) {
            if (!appendf(cmd, sizeof(cmd), "--gain-db %lld ", cfg.gain_db)) {
                fprintf(stderr, "ERROR: command buffer overflow\n");
                return 1;
            }
        } else {
            if (!appendf(cmd, sizeof(cmd), "--gain-mode \"%s\" ", cfg.gain_mode)) {
                fprintf(stderr, "ERROR: command buffer overflow\n");
                return 1;
            }
        }

        int ret = run_command("Activity monitor", cmd, cfg.dry_run);
        if (ret != 0) {
            return 1;
        }
    } else {
        printf("Skipping activity monitor. Expected activity CSV: %s\n", activity_csv);
    }

    {
        char cmd[8192];
        cmd[0] = '\0';

        if (!appendf(cmd, sizeof(cmd),
                     "call \"%s\\pluto_activity_summary.exe\" "
                     "--in \"%s\" "
                     "--out \"%s\" "
                     "--sort \"%s\" "
                     "--min-hits %d ",
                     tool_dir,
                     activity_csv,
                     summary_csv,
                     cfg.summary_sort,
                     cfg.min_active_hits)) {
            fprintf(stderr, "ERROR: command buffer overflow\n");
            return 1;
        }

        int ret = run_command("Activity summary", cmd, cfg.dry_run);
        if (ret != 0) {
            return 1;
        }
    }

    if (cfg.generate_report) {
        char cmd[8192];
        cmd[0] = '\0';

        if (!appendf(cmd, sizeof(cmd),
                     "call \"%s\\pluto_session_report.exe\" "
                     "--prefix \"%s\" "
                     "--out \"%s\" "
                     "--title \"%s\" "
                     "--raw \"%s\" "
                     "--grouped \"%s\" "
                     "--activity \"%s\" "
                     "--summary \"%s\" "
                     "--max-rows %d ",
                     tool_dir,
                     prefix,
                     report_file,
                     cfg.report_title,
                     raw_csv,
                     grouped_csv,
                     activity_csv,
                     summary_csv,
                     cfg.report_max_rows)) {
            fprintf(stderr, "ERROR: command buffer overflow\n");
            return 1;
        }

        int ret = run_command("Session report", cmd, cfg.dry_run);
        if (ret != 0) {
            return 1;
        }
    }

    printf("\n");
    printf("Scan session complete.\n");
    printf("Raw CSV:      %s\n", raw_csv);
    printf("Grouped CSV:  %s\n", grouped_csv);
    printf("Activity CSV: %s\n", activity_csv);
    printf("Summary CSV:  %s\n", summary_csv);

    if (cfg.generate_report) {
        printf("Report HTML:  %s\n", report_file);
    }

    printf("\n");

    return 0;
}
