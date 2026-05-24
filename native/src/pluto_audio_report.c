#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
   pluto_audio_report.c

   Generates an HTML report from pluto_audio_monitor audio_log.csv.

   Default:
     input:  audio_log.csv
     output: audio_report.html

   Recommended from repo root:
     ./build/native/pluto_audio_report.exe \
       --in sessions/audio_log.csv \
       --out sessions/audio_report.html

   Recommended from sessions folder:
     ../build/native/pluto_audio_report.exe \
       --in audio_log.csv \
       --out audio_report.html
*/

#define MAX_LINE 8192
#define MAX_FIELDS 64

typedef struct {
    const char *input_path;
    const char *output_path;
    const char *title;
    bool open_hint;
} app_config_t;

typedef struct {
    char *timestamp;
    char *preset;
    char *mode;
    char *freq_hz;
    char *seconds;
    char *wav_path;
    char *sample_rate_hz;
    char *audio_rate_hz;
    char *gain_mode;
    char *manual_gain_db;
    char *squelch_enabled;
    char *squelch_db;
    char *rf_avg_dbfs;
    char *rf_min_dbfs;
    char *rf_max_dbfs;
    char *audio_rms;
    char *audio_peak;
    char *squelch_open_pct;
    char *audio_samples;
} audio_row_t;

static void print_usage(const char *prog)
{
    printf("\n");
    printf("Pluto+ Audio Report Generator\n");
    printf("\n");
    printf("Usage:\n");
    printf("  %s --in <audio_log.csv> --out <audio_report.html> [options]\n", prog);
    printf("\n");
    printf("Options:\n");
    printf("  --in <file>       Input CSV, default audio_log.csv\n");
    printf("  --out <file>      Output HTML, default audio_report.html\n");
    printf("  --title <text>    Report title, default Pluto+ Audio Recording Report\n");
    printf("  --help            Show this help\n");
    printf("\n");
}

static char *xstrdup(const char *s)
{
    if (!s) {
        s = "";
    }

    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) {
        fprintf(stderr, "ERROR: out of memory\n");
        exit(1);
    }

    memcpy(out, s, n + 1);
    return out;
}

static void trim_in_place(char *s)
{
    if (!s) {
        return;
    }

    char *p = s;
    while (*p && isspace((unsigned char)*p)) {
        p++;
    }

    if (p != s) {
        memmove(s, p, strlen(p) + 1);
    }

    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) {
        s[n - 1] = '\0';
        n--;
    }
}

static int split_csv_simple(char *line, char **fields, int max_fields)
{
    int count = 0;
    char *p = line;
    bool in_quotes = false;
    char *field_start = p;

    while (*p && count < max_fields) {
        if (*p == '"') {
            in_quotes = !in_quotes;
        } else if (*p == ',' && !in_quotes) {
            *p = '\0';
            fields[count++] = field_start;
            field_start = p + 1;
        }
        p++;
    }

    if (count < max_fields) {
        fields[count++] = field_start;
    }

    for (int i = 0; i < count; i++) {
        trim_in_place(fields[i]);

        size_t n = strlen(fields[i]);
        if (n >= 2 && fields[i][0] == '"' && fields[i][n - 1] == '"') {
            fields[i][n - 1] = '\0';
            fields[i]++;
        }
    }

    return count;
}

static const char *field_or_empty(char **fields, int count, int index)
{
    if (index < 0 || index >= count || !fields[index]) {
        return "";
    }

    return fields[index];
}

static audio_row_t row_from_fields(char **fields, int count)
{
    audio_row_t r;

    r.timestamp = xstrdup(field_or_empty(fields, count, 0));
    r.preset = xstrdup(field_or_empty(fields, count, 1));
    r.mode = xstrdup(field_or_empty(fields, count, 2));
    r.freq_hz = xstrdup(field_or_empty(fields, count, 3));
    r.seconds = xstrdup(field_or_empty(fields, count, 4));
    r.wav_path = xstrdup(field_or_empty(fields, count, 5));
    r.sample_rate_hz = xstrdup(field_or_empty(fields, count, 6));
    r.audio_rate_hz = xstrdup(field_or_empty(fields, count, 7));
    r.gain_mode = xstrdup(field_or_empty(fields, count, 8));
    r.manual_gain_db = xstrdup(field_or_empty(fields, count, 9));
    r.squelch_enabled = xstrdup(field_or_empty(fields, count, 10));
    r.squelch_db = xstrdup(field_or_empty(fields, count, 11));
    r.rf_avg_dbfs = xstrdup(field_or_empty(fields, count, 12));
    r.rf_min_dbfs = xstrdup(field_or_empty(fields, count, 13));
    r.rf_max_dbfs = xstrdup(field_or_empty(fields, count, 14));
    r.audio_rms = xstrdup(field_or_empty(fields, count, 15));
    r.audio_peak = xstrdup(field_or_empty(fields, count, 16));
    r.squelch_open_pct = xstrdup(field_or_empty(fields, count, 17));
    r.audio_samples = xstrdup(field_or_empty(fields, count, 18));

    return r;
}

static void free_row(audio_row_t *r)
{
    free(r->timestamp);
    free(r->preset);
    free(r->mode);
    free(r->freq_hz);
    free(r->seconds);
    free(r->wav_path);
    free(r->sample_rate_hz);
    free(r->audio_rate_hz);
    free(r->gain_mode);
    free(r->manual_gain_db);
    free(r->squelch_enabled);
    free(r->squelch_db);
    free(r->rf_avg_dbfs);
    free(r->rf_min_dbfs);
    free(r->rf_max_dbfs);
    free(r->audio_rms);
    free(r->audio_peak);
    free(r->squelch_open_pct);
    free(r->audio_samples);
}

static void html_escape(FILE *fp, const char *s)
{
    if (!s) {
        return;
    }

    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '&':
            fputs("&amp;", fp);
            break;
        case '<':
            fputs("&lt;", fp);
            break;
        case '>':
            fputs("&gt;", fp);
            break;
        case '"':
            fputs("&quot;", fp);
            break;
        case '\'':
            fputs("&#39;", fp);
            break;
        default:
            fputc(*p, fp);
            break;
        }
    }
}

static bool parse_args(int argc, char **argv, app_config_t *cfg)
{
    cfg->input_path = "audio_log.csv";
    cfg->output_path = "audio_report.html";
    cfg->title = "Pluto+ Audio Recording Report";
    cfg->open_hint = true;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];

        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            print_usage(argv[0]);
            exit(0);
        } else if (strcmp(arg, "--in") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "ERROR: --in requires a file\n");
                return false;
            }
            cfg->input_path = argv[i];
        } else if (strcmp(arg, "--out") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "ERROR: --out requires a file\n");
                return false;
            }
            cfg->output_path = argv[i];
        } else if (strcmp(arg, "--title") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "ERROR: --title requires text\n");
                return false;
            }
            cfg->title = argv[i];
        } else {
            fprintf(stderr, "ERROR: Unknown option: %s\n", arg);
            return false;
        }
    }

    return true;
}

static const char *mode_label(const char *mode)
{
    if (!mode) {
        return "";
    }

    if (strcmp(mode, "nfm") == 0) {
        return "NFM";
    }

    if (strcmp(mode, "am") == 0) {
        return "AM";
    }

    if (strcmp(mode, "wbfm") == 0) {
        return "WBFM";
    }

    return mode;
}

static void write_html_header(FILE *out, const app_config_t *cfg)
{
    fputs("<!doctype html>\n", out);
    fputs("<html lang=\"en\">\n<head>\n", out);
    fputs("<meta charset=\"utf-8\">\n", out);
    fputs("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n", out);
    fputs("<title>", out);
    html_escape(out, cfg->title);
    fputs("</title>\n", out);
    fputs("<style>\n", out);
    fputs("body{font-family:Segoe UI,Arial,sans-serif;margin:24px;background:#f7f7f7;color:#222;}\n", out);
    fputs("h1{margin-bottom:4px;} .meta{color:#555;margin-bottom:20px;}\n", out);
    fputs(".cards{display:flex;gap:12px;flex-wrap:wrap;margin:16px 0;}\n", out);
    fputs(".card{background:white;border:1px solid #ddd;border-radius:8px;padding:12px 16px;box-shadow:0 1px 3px #ddd;}\n", out);
    fputs(".card .num{font-size:24px;font-weight:600;}\n", out);
    fputs("table{border-collapse:collapse;width:100%;background:white;border:1px solid #ddd;}\n", out);
    fputs("th,td{border-bottom:1px solid #e5e5e5;padding:8px;text-align:left;vertical-align:top;font-size:13px;}\n", out);
    fputs("th{background:#eee;position:sticky;top:0;z-index:1;}\n", out);
    fputs("tr:hover{background:#fafafa;} audio{width:260px;max-width:100%;}\n", out);
    fputs(".mode{font-weight:600;} .small{font-size:12px;color:#666;}\n", out);
    fputs(".missing{color:#9a3412;font-weight:600;}\n", out);
    fputs("</style>\n</head>\n<body>\n", out);
    fputs("<h1>", out);
    html_escape(out, cfg->title);
    fputs("</h1>\n", out);
    fputs("<div class=\"meta\">Input CSV: <code>", out);
    html_escape(out, cfg->input_path);
    fputs("</code></div>\n", out);
}

static void write_html_footer(FILE *out)
{
    fputs("</body>\n</html>\n", out);
}

static double to_double(const char *s, double fallback)
{
    if (!s || !*s) {
        return fallback;
    }

    char *end = NULL;
    errno = 0;
    double v = strtod(s, &end);

    if (errno != 0 || end == s) {
        return fallback;
    }

    return v;
}

int main(int argc, char **argv)
{
    app_config_t cfg;

    if (!parse_args(argc, argv, &cfg)) {
        print_usage(argv[0]);
        return 1;
    }

    FILE *in = fopen(cfg.input_path, "r");
    if (!in) {
        fprintf(stderr, "ERROR: could not open input CSV: %s\n", cfg.input_path);
        return 1;
    }

    FILE *out = fopen(cfg.output_path, "w");
    if (!out) {
        fprintf(stderr, "ERROR: could not open output HTML: %s\n", cfg.output_path);
        fclose(in);
        return 1;
    }

    audio_row_t *rows = NULL;
    size_t count = 0;
    size_t capacity = 0;

    char line[MAX_LINE];

    /*
       Skip header.
    */
    if (!fgets(line, sizeof(line), in)) {
        write_html_header(out, &cfg);
        fputs("<p class=\"missing\">No rows found. The CSV is empty.</p>\n", out);
        write_html_footer(out);
        fclose(in);
        fclose(out);
        printf("Audio report written: %s\n", cfg.output_path);
        return 0;
    }

    while (fgets(line, sizeof(line), in)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[len - 1] = '\0';
            len--;
        }

        if (len == 0) {
            continue;
        }

        char *fields[MAX_FIELDS] = {0};
        int field_count = split_csv_simple(line, fields, MAX_FIELDS);

        if (field_count < 6) {
            continue;
        }

        if (count == capacity) {
            capacity = capacity == 0 ? 32 : capacity * 2;
            audio_row_t *new_rows = (audio_row_t *)realloc(rows, capacity * sizeof(audio_row_t));
            if (!new_rows) {
                fprintf(stderr, "ERROR: out of memory\n");
                fclose(in);
                fclose(out);
                return 1;
            }
            rows = new_rows;
        }

        rows[count++] = row_from_fields(fields, field_count);
    }

    fclose(in);

    double min_rf = 999.0;
    double max_rf = -999.0;
    double sum_rf = 0.0;
    double max_audio_peak = 0.0;
    size_t rf_count = 0;
    size_t nfm_count = 0;
    size_t am_count = 0;
    size_t wbfm_count = 0;

    for (size_t i = 0; i < count; i++) {
        audio_row_t *r = &rows[i];

        if (strcmp(r->mode, "nfm") == 0) {
            nfm_count++;
        } else if (strcmp(r->mode, "am") == 0) {
            am_count++;
        } else if (strcmp(r->mode, "wbfm") == 0) {
            wbfm_count++;
        }

        double rf = to_double(r->rf_avg_dbfs, -999.0);
        if (rf > -998.0) {
            if (rf < min_rf) {
                min_rf = rf;
            }
            if (rf > max_rf) {
                max_rf = rf;
            }
            sum_rf += rf;
            rf_count++;
        }

        double peak = to_double(r->audio_peak, 0.0);
        if (peak > max_audio_peak) {
            max_audio_peak = peak;
        }
    }

    write_html_header(out, &cfg);

    fputs("<div class=\"cards\">\n", out);
    fprintf(out, "<div class=\"card\"><div class=\"num\">%zu</div><div>Recordings</div></div>\n", count);
    fprintf(out, "<div class=\"card\"><div class=\"num\">%zu</div><div>NFM</div></div>\n", nfm_count);
    fprintf(out, "<div class=\"card\"><div class=\"num\">%zu</div><div>AM</div></div>\n", am_count);
    fprintf(out, "<div class=\"card\"><div class=\"num\">%zu</div><div>WBFM</div></div>\n", wbfm_count);

    if (rf_count > 0) {
        fprintf(out, "<div class=\"card\"><div class=\"num\">%.1f</div><div>Avg RF dBFS</div></div>\n", sum_rf / (double)rf_count);
        fprintf(out, "<div class=\"card\"><div class=\"num\">%.1f</div><div>Max RF dBFS</div></div>\n", max_rf);
    }

    fprintf(out, "<div class=\"card\"><div class=\"num\">%.3f</div><div>Max audio peak</div></div>\n", max_audio_peak);
    fputs("</div>\n", out);

    if (count == 0) {
        fputs("<p class=\"missing\">No recordings found in the CSV.</p>\n", out);
    } else {
        fputs("<table>\n", out);
        fputs("<thead><tr>", out);
        fputs("<th>#</th><th>Time</th><th>Mode</th><th>Preset</th><th>Frequency</th><th>Seconds</th><th>RF Avg</th><th>Squelch Open</th><th>Audio Peak</th><th>WAV</th><th>Player</th>", out);
        fputs("</tr></thead>\n<tbody>\n", out);

        for (size_t i = 0; i < count; i++) {
            audio_row_t *r = &rows[i];

            fprintf(out, "<tr><td>%zu</td>", i + 1);

            fputs("<td>", out);
            html_escape(out, r->timestamp);
            fputs("</td>", out);

            fputs("<td class=\"mode\">", out);
            html_escape(out, mode_label(r->mode));
            fputs("</td>", out);

            fputs("<td>", out);
            html_escape(out, r->preset);
            fputs("</td>", out);

            fputs("<td>", out);
            html_escape(out, r->freq_hz);
            fputs("</td>", out);

            fputs("<td>", out);
            html_escape(out, r->seconds);
            fputs("</td>", out);

            fputs("<td>", out);
            html_escape(out, r->rf_avg_dbfs);
            fputs(" dBFS</td>", out);

            fputs("<td>", out);
            html_escape(out, r->squelch_open_pct);
            fputs("%</td>", out);

            fputs("<td>", out);
            html_escape(out, r->audio_peak);
            fputs("</td>", out);

            fputs("<td><a href=\"", out);
            html_escape(out, r->wav_path);
            fputs("\">", out);
            html_escape(out, r->wav_path);
            fputs("</a></td>", out);

            fputs("<td><audio controls preload=\"none\" src=\"", out);
            html_escape(out, r->wav_path);
            fputs("\"></audio></td>", out);

            fputs("</tr>\n", out);
        }

        fputs("</tbody>\n</table>\n", out);
    }

    fputs("<p class=\"small\">Generated by <code>pluto_audio_report.exe</code>.</p>\n", out);

    write_html_footer(out);
    fclose(out);

    for (size_t i = 0; i < count; i++) {
        free_row(&rows[i]);
    }
    free(rows);

    printf("Audio report written: %s\n", cfg.output_path);
    printf("Rows: %zu\n", count);

    return 0;
}
