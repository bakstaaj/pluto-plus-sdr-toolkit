#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    const char *prefix;
    const char *out_file;
    const char *title;

    const char *raw_csv;
    const char *grouped_csv;
    const char *activity_csv;
    const char *summary_csv;

    int max_rows;
} app_config_t;

static void print_usage(const char *prog)
{
    printf("\n");
    printf("Pluto+ Session Report Generator\n");
    printf("\n");
    printf("Usage:\n");
    printf("  %s --prefix <session_prefix> [options]\n", prog);
    printf("\n");
    printf("Default input files:\n");
    printf("  <prefix>_raw.csv\n");
    printf("  <prefix>_grouped.csv\n");
    printf("  <prefix>_activity.csv\n");
    printf("  <prefix>_summary.csv\n");
    printf("\n");
    printf("Options:\n");
    printf("  --prefix <name>       Session prefix, for example 2m, airband, noaa\n");
    printf("  --out <file>          Output HTML file, default <prefix>_report.html\n");
    printf("  --title <text>        Report title\n");
    printf("  --raw <file>          Override raw scan CSV file\n");
    printf("  --grouped <file>      Override grouped CSV file\n");
    printf("  --activity <file>     Override activity CSV file\n");
    printf("  --summary <file>      Override summary CSV file\n");
    printf("  --max-rows <n>        Max rows shown per table, default 200\n");
    printf("  --help                Show this help\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s --prefix 2m\n", prog);
    printf("  %s --prefix airband --out airband_report.html\n", prog);
    printf("  %s --prefix noaa --max-rows 500\n", prog);
    printf("\n");
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
    cfg->prefix = NULL;
    cfg->out_file = NULL;
    cfg->title = "Pluto+ SDR Scan Session Report";

    cfg->raw_csv = NULL;
    cfg->grouped_csv = NULL;
    cfg->activity_csv = NULL;
    cfg->summary_csv = NULL;

    cfg->max_rows = 200;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];

        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            print_usage(argv[0]);
            exit(0);
        } else if (strcmp(arg, "--prefix") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "ERROR: --prefix requires a value\n");
                return false;
            }
            cfg->prefix = argv[i];
        } else if (strcmp(arg, "--out") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "ERROR: --out requires a filename\n");
                return false;
            }
            cfg->out_file = argv[i];
        } else if (strcmp(arg, "--title") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "ERROR: --title requires text\n");
                return false;
            }
            cfg->title = argv[i];
        } else if (strcmp(arg, "--raw") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "ERROR: --raw requires a filename\n");
                return false;
            }
            cfg->raw_csv = argv[i];
        } else if (strcmp(arg, "--grouped") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "ERROR: --grouped requires a filename\n");
                return false;
            }
            cfg->grouped_csv = argv[i];
        } else if (strcmp(arg, "--activity") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "ERROR: --activity requires a filename\n");
                return false;
            }
            cfg->activity_csv = argv[i];
        } else if (strcmp(arg, "--summary") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "ERROR: --summary requires a filename\n");
                return false;
            }
            cfg->summary_csv = argv[i];
        } else if (strcmp(arg, "--max-rows") == 0) {
            if (++i >= argc || !parse_int_value(argv[i], &cfg->max_rows)) {
                fprintf(stderr, "ERROR: --max-rows requires an integer value\n");
                return false;
            }
        } else {
            fprintf(stderr, "ERROR: Unknown option: %s\n", arg);
            return false;
        }
    }

    if (!cfg->prefix) {
        fprintf(stderr, "ERROR: --prefix is required\n");
        return false;
    }

    if (cfg->max_rows <= 0) {
        fprintf(stderr, "ERROR: --max-rows must be greater than zero\n");
        return false;
    }

    return true;
}

static bool file_exists(const char *filename)
{
    FILE *f = fopen(filename, "r");

    if (!f) {
        return false;
    }

    fclose(f);
    return true;
}

static void html_escape(FILE *out, const char *s)
{
    for (; *s; s++) {
        switch (*s) {
            case '&':
                fputs("&amp;", out);
                break;
            case '<':
                fputs("&lt;", out);
                break;
            case '>':
                fputs("&gt;", out);
                break;
            case '"':
                fputs("&quot;", out);
                break;
            case '\'':
                fputs("&#39;", out);
                break;
            default:
                fputc(*s, out);
                break;
        }
    }
}

static void trim_in_place(char *s)
{
    char *start = s;

    while (*start == ' ' || *start == '\t') {
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

static long count_csv_rows(const char *filename)
{
    FILE *f = fopen(filename, "r");

    if (!f) {
        return -1;
    }

    char line[8192];
    long lines = 0;

    while (fgets(line, sizeof(line), f)) {
        if (line[0] != '\0' && line[0] != '\r' && line[0] != '\n') {
            lines++;
        }
    }

    fclose(f);

    if (lines <= 0) {
        return 0;
    }

    return lines - 1;
}

static void write_csv_row_as_html(FILE *out, char *line, bool header)
{
    line[strcspn(line, "\r\n")] = '\0';

    fprintf(out, "<tr>");

    char *field_start = line;
    char *p = line;

    while (1) {
        if (*p == ',' || *p == '\0') {
            char saved = *p;
            *p = '\0';

            trim_in_place(field_start);

            if (header) {
                fprintf(out, "<th>");
                html_escape(out, field_start);
                fprintf(out, "</th>");
            } else {
                fprintf(out, "<td>");
                html_escape(out, field_start);
                fprintf(out, "</td>");
            }

            if (saved == '\0') {
                break;
            }

            field_start = p + 1;
            *p = saved;
        }

        p++;
    }

    fprintf(out, "</tr>\n");
}

static void render_csv_section(
    FILE *out,
    const char *section_title,
    const char *filename,
    int max_rows,
    const char *note)
{
    fprintf(out, "<section>\n");
    fprintf(out, "<h2>");
    html_escape(out, section_title);
    fprintf(out, "</h2>\n");

    fprintf(out, "<p class=\"file-name\">File: <code>");
    html_escape(out, filename);
    fprintf(out, "</code></p>\n");

    if (!file_exists(filename)) {
        fprintf(out, "<div class=\"warning\">File not found. This section was skipped.</div>\n");
        fprintf(out, "</section>\n");
        return;
    }

    long row_count = count_csv_rows(filename);

    fprintf(out, "<p>Total data rows: <strong>%ld</strong></p>\n", row_count);

    if (note && note[0]) {
        fprintf(out, "<p class=\"note\">");
        html_escape(out, note);
        fprintf(out, "</p>\n");
    }

    FILE *f = fopen(filename, "r");

    if (!f) {
        fprintf(out, "<div class=\"warning\">Could not open file.</div>\n");
        fprintf(out, "</section>\n");
        return;
    }

    char line[8192];
    bool have_header = false;
    int rendered_rows = 0;

    fprintf(out, "<div class=\"table-wrap\">\n");
    fprintf(out, "<table>\n");

    if (fgets(line, sizeof(line), f)) {
        write_csv_row_as_html(out, line, true);
        have_header = true;
    }

    if (!have_header) {
        fprintf(out, "</table>\n");
        fprintf(out, "</div>\n");
        fprintf(out, "<div class=\"warning\">CSV file is empty.</div>\n");
        fclose(f);
        fprintf(out, "</section>\n");
        return;
    }

    while (rendered_rows < max_rows && fgets(line, sizeof(line), f)) {
        if (line[0] == '\0' || line[0] == '\r' || line[0] == '\n') {
            continue;
        }

        write_csv_row_as_html(out, line, false);
        rendered_rows++;
    }

    fprintf(out, "</table>\n");
    fprintf(out, "</div>\n");

    if (row_count > max_rows) {
        fprintf(out,
                "<p class=\"note\">Showing first %d rows only. Open the CSV file for the complete data.</p>\n",
                max_rows);
    }

    fclose(f);

    fprintf(out, "</section>\n");
}

static void current_time_string(char *buf, size_t buf_size)
{
    time_t now = time(NULL);
    struct tm tmv;

#ifdef _WIN32
    localtime_s(&tmv, &now);
#else
    localtime_r(&now, &tmv);
#endif

    strftime(buf, buf_size, "%Y-%m-%d %H:%M:%S", &tmv);
}

static void write_html_header(FILE *out, const app_config_t *cfg, const char *generated_at)
{
    fprintf(out, "<!DOCTYPE html>\n");
    fprintf(out, "<html lang=\"en\">\n");
    fprintf(out, "<head>\n");
    fprintf(out, "<meta charset=\"utf-8\">\n");
    fprintf(out, "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n");
    fprintf(out, "<title>");
    html_escape(out, cfg->title);
    fprintf(out, "</title>\n");

    fprintf(out, "<style>\n");
    fprintf(out, "body { font-family: Segoe UI, Arial, sans-serif; margin: 24px; background: #f5f7fb; color: #1f2933; }\n");
    fprintf(out, "header { background: #ffffff; padding: 20px 24px; border-radius: 12px; box-shadow: 0 2px 10px rgba(0,0,0,0.08); margin-bottom: 20px; }\n");
    fprintf(out, "section { background: #ffffff; padding: 18px 20px; border-radius: 12px; box-shadow: 0 2px 10px rgba(0,0,0,0.08); margin-bottom: 20px; }\n");
    fprintf(out, "h1 { margin: 0 0 8px 0; font-size: 28px; }\n");
    fprintf(out, "h2 { margin-top: 0; border-bottom: 1px solid #d8dee9; padding-bottom: 8px; }\n");
    fprintf(out, ".meta { color: #52616b; margin: 4px 0; }\n");
    fprintf(out, ".file-name { color: #52616b; }\n");
    fprintf(out, ".note { color: #52616b; font-style: italic; }\n");
    fprintf(out, ".warning { background: #fff4cc; color: #7a5200; padding: 10px 12px; border-radius: 8px; margin: 10px 0; }\n");
    fprintf(out, ".summary-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(220px, 1fr)); gap: 12px; }\n");
    fprintf(out, ".card { background: #eef3f8; padding: 12px; border-radius: 10px; }\n");
    fprintf(out, ".card-title { color: #52616b; font-size: 13px; }\n");
    fprintf(out, ".card-value { font-size: 22px; font-weight: 700; margin-top: 4px; }\n");
    fprintf(out, ".table-wrap { overflow-x: auto; }\n");
    fprintf(out, "table { border-collapse: collapse; width: 100%; font-size: 13px; }\n");
    fprintf(out, "th, td { border: 1px solid #d8dee9; padding: 6px 8px; text-align: left; white-space: nowrap; }\n");
    fprintf(out, "th { background: #e8eef5; position: sticky; top: 0; }\n");
    fprintf(out, "tr:nth-child(even) td { background: #f8fafc; }\n");
    fprintf(out, "code { background: #eef3f8; padding: 2px 5px; border-radius: 5px; }\n");
    fprintf(out, "</style>\n");

    fprintf(out, "</head>\n");
    fprintf(out, "<body>\n");

    fprintf(out, "<header>\n");
    fprintf(out, "<h1>");
    html_escape(out, cfg->title);
    fprintf(out, "</h1>\n");
    fprintf(out, "<p class=\"meta\">Generated: ");
    html_escape(out, generated_at);
    fprintf(out, "</p>\n");
    fprintf(out, "<p class=\"meta\">Session prefix: <code>");
    html_escape(out, cfg->prefix);
    fprintf(out, "</code></p>\n");
    fprintf(out, "</header>\n");
}

static void write_html_footer(FILE *out)
{
    fprintf(out, "</body>\n");
    fprintf(out, "</html>\n");
}

static void render_overview(FILE *out, const app_config_t *cfg)
{
    long raw_rows = count_csv_rows(cfg->raw_csv);
    long grouped_rows = count_csv_rows(cfg->grouped_csv);
    long activity_rows = count_csv_rows(cfg->activity_csv);
    long summary_rows = count_csv_rows(cfg->summary_csv);

    fprintf(out, "<section>\n");
    fprintf(out, "<h2>Session Overview</h2>\n");
    fprintf(out, "<div class=\"summary-grid\">\n");

    fprintf(out, "<div class=\"card\"><div class=\"card-title\">Raw detections</div><div class=\"card-value\">%ld</div></div>\n",
            raw_rows >= 0 ? raw_rows : 0);

    fprintf(out, "<div class=\"card\"><div class=\"card-title\">Grouped signals</div><div class=\"card-value\">%ld</div></div>\n",
            grouped_rows >= 0 ? grouped_rows : 0);

    fprintf(out, "<div class=\"card\"><div class=\"card-title\">Activity checks</div><div class=\"card-value\">%ld</div></div>\n",
            activity_rows >= 0 ? activity_rows : 0);

    fprintf(out, "<div class=\"card\"><div class=\"card-title\">Summary rows</div><div class=\"card-value\">%ld</div></div>\n",
            summary_rows >= 0 ? summary_rows : 0);

    fprintf(out, "</div>\n");
    fprintf(out, "</section>\n");
}

int main(int argc, char **argv)
{
    app_config_t cfg;

    if (!parse_args(argc, argv, &cfg)) {
        print_usage(argv[0]);
        return 1;
    }

    char default_out[512];
    char default_raw[512];
    char default_grouped[512];
    char default_activity[512];
    char default_summary[512];

    snprintf(default_out, sizeof(default_out), "%s_report.html", cfg.prefix);
    snprintf(default_raw, sizeof(default_raw), "%s_raw.csv", cfg.prefix);
    snprintf(default_grouped, sizeof(default_grouped), "%s_grouped.csv", cfg.prefix);
    snprintf(default_activity, sizeof(default_activity), "%s_activity.csv", cfg.prefix);
    snprintf(default_summary, sizeof(default_summary), "%s_summary.csv", cfg.prefix);

    if (!cfg.out_file) {
        cfg.out_file = default_out;
    }

    if (!cfg.raw_csv) {
        cfg.raw_csv = default_raw;
    }

    if (!cfg.grouped_csv) {
        cfg.grouped_csv = default_grouped;
    }

    if (!cfg.activity_csv) {
        cfg.activity_csv = default_activity;
    }

    if (!cfg.summary_csv) {
        cfg.summary_csv = default_summary;
    }

    printf("\n");
    printf("Pluto+ Session Report Generator\n");
    printf("-------------------------------\n");
    printf("Prefix:        %s\n", cfg.prefix);
    printf("Output HTML:   %s\n", cfg.out_file);
    printf("Raw CSV:       %s\n", cfg.raw_csv);
    printf("Grouped CSV:   %s\n", cfg.grouped_csv);
    printf("Activity CSV:  %s\n", cfg.activity_csv);
    printf("Summary CSV:   %s\n", cfg.summary_csv);
    printf("Max rows:      %d\n", cfg.max_rows);
    printf("\n");

    FILE *out = fopen(cfg.out_file, "w");

    if (!out) {
        fprintf(stderr, "ERROR: Could not open output file: %s\n", cfg.out_file);
        return 1;
    }

    char generated_at[64];
    current_time_string(generated_at, sizeof(generated_at));

    write_html_header(out, &cfg, generated_at);

    render_overview(out, &cfg);

    render_csv_section(
        out,
        "Activity Summary",
        cfg.summary_csv,
        cfg.max_rows,
        "This is usually the most useful table. It summarizes which frequencies were active and how often."
    );

    render_csv_section(
        out,
        "Grouped Signals",
        cfg.grouped_csv,
        cfg.max_rows,
        "Grouped detections combine nearby FFT peaks into practical signal candidates."
    );

    render_csv_section(
        out,
        "Activity Log",
        cfg.activity_csv,
        cfg.max_rows,
        "This table shows each monitoring check. Large sessions may have many rows."
    );

    render_csv_section(
        out,
        "Raw Scan Detections",
        cfg.raw_csv,
        cfg.max_rows,
        "These are the direct FFT peak detections from the sweep scanner."
    );

    write_html_footer(out);
    fclose(out);

    printf("Report written: %s\n\n", cfg.out_file);

    return 0;
}
