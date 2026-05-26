#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
   pluto_band_scan.c

   Preset band scanner wrapper.

   Defaults/assumptions:
     - Target hardware is the user's Pluto+ with RX coverage down to at least 70 MHz.
     - FM broadcast, 88-108 MHz, is a first-class preset for testing.
     - Uses "call" in generated commands so Windows cmd.exe handles quoted EXE paths.
*/

#define PLUTO_PLUS_DEFAULT_MIN_HZ 70000000LL

typedef struct {
    const char *id;
    const char *description;
    long long start_hz;
    long long stop_hz;
    long long step_hz;
    long long rate_hz;
    long long bw_hz;
    int fft_size;
    int averages;
    double threshold_db;
    double merge_hz;
    double dc_exclude_hz;
} band_preset_t;

typedef struct {
    const char *band;
    const char *uri;
    const char *tool_dir;
    const char *out_prefix;
    const char *gain_mode;
    const char *rx_mode;
    const char *rx_combine;
    long long gain_db;
    bool use_gain_db;
    bool no_group;
    bool dry_run;
    bool sort_snr;
    double threshold_override;
    bool has_threshold_override;
    int avg_override;
    bool has_avg_override;
} app_config_t;

static const band_preset_t presets[] = {
    {"fm", "FM broadcast band, 88-108 MHz", 88000000LL, 108000000LL, 1000000LL, 2000000LL, 2000000LL, 32768, 4, 10.0, 200000.0, 20000.0},
    {"airband", "VHF aviation airband, 118-137 MHz", 118000000LL, 137000000LL, 500000LL, 1000000LL, 1000000LL, 16384, 4, 8.0, 25000.0, 10000.0},
    {"2m", "2 meter amateur radio band, 144-148 MHz", 144000000LL, 148000000LL, 500000LL, 1000000LL, 1000000LL, 16384, 4, 8.0, 12500.0, 10000.0},
    {"noaa", "NOAA weather radio channels, 162.400-162.550 MHz", 162400000LL, 162550000LL, 250000LL, 1000000LL, 1000000LL, 16384, 8, 8.0, 12500.0, 10000.0},
    {"70cm", "70 centimeter amateur radio band, 420-450 MHz", 420000000LL, 450000000LL, 1000000LL, 2000000LL, 2000000LL, 32768, 4, 8.0, 12500.0, 15000.0},
    {"ism915", "ISM 915 MHz band, 902-928 MHz", 902000000LL, 928000000LL, 1000000LL, 2000000LL, 2000000LL, 32768, 4, 10.0, 50000.0, 15000.0}
};

static const size_t preset_count = sizeof(presets) / sizeof(presets[0]);

static bool parse_ll(const char *text, long long *value) {
    char *end = NULL; errno = 0; long long v = strtoll(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0') return false;
    *value = v; return true;
}

static bool parse_double_value(const char *text, double *value) {
    char *end = NULL; errno = 0; double v = strtod(text, &end);
    if (errno != 0 || end == text || *end != '\0') return false;
    *value = v; return true;
}

static bool parse_int_value(const char *text, int *value) {
    char *end = NULL; errno = 0; long v = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0') return false;
    *value = (int)v; return true;
}

static void print_usage(const char *prog) {
    printf("\nPluto+ Preset Band Scanner\n\n");
    printf("Default hardware assumption: Pluto+ RX minimum %.3f MHz\n", (double)PLUTO_PLUS_DEFAULT_MIN_HZ / 1e6);
    printf("Usage:\n  %s --band <preset> [options]\n\n", prog);
    printf("Band presets:\n");
    for (size_t i = 0; i < preset_count; i++) printf("  %-8s  %s\n", presets[i].id, presets[i].description);
    printf("\nOptions:\n");
    printf("  --band <name>          Preset band name\n");
    printf("  --uri <uri>            IIO URI, default ip:192.168.2.1\n");
    printf("  --tool-dir <dir>       Directory containing scanner EXEs, default derived from this EXE path\n");
    printf("  --out-prefix <prefix>  Output prefix, default is band name\n");
    printf("  --threshold-db <db>    Override sweep detection threshold\n");
    printf("  --avg <count>          Override FFT averages\n");
    printf("  --gain-mode <mode>     slow_attack, fast_attack, manual, default slow_attack\n");
    printf("  --gain-db <db>         Manual gain dB, implies manual gain mode\n");
    printf("  --rx-mode <mode>       auto, single, dual. Default auto\n");
    printf("  --rx-combine <mode>    max, average, separate. Default max\n");
    printf("  --sort snr             Grouped CSV sorted by strongest SNR instead of frequency\n");
    printf("  --no-group             Only run sweep scanner, do not group CSV\n");
    printf("  --dry-run              Print commands but do not execute them\n");
    printf("  --help                 Show this help\n\n");
    printf("Examples:\n");
    printf("  %s --band fm\n", prog);
    printf("  %s --band 2m\n", prog);
    printf("  %s --band airband --threshold-db 10\n", prog);
    printf("  %s --band 70cm --gain-db 40 --sort snr\n\n", prog);
}

static void get_default_tool_dir(const char *argv0, char *out, size_t out_size) {
    const char *last_slash = strrchr(argv0, '/');
    const char *last_backslash = strrchr(argv0, '\\');
    const char *last = last_slash;
    if (last_backslash && (!last || last_backslash > last)) last = last_backslash;
    if (!last) { snprintf(out, out_size, "."); return; }
    size_t len = (size_t)(last - argv0);
    if (len == 0 || len >= out_size) { snprintf(out, out_size, "."); return; }
    memcpy(out, argv0, len); out[len] = '\0';
}

static const band_preset_t *find_preset(const char *id) {
    for (size_t i = 0; i < preset_count; i++) if (strcmp(presets[i].id, id) == 0) return &presets[i];
    return NULL;
}

static bool parse_args(int argc, char **argv, app_config_t *cfg) {
    cfg->band = NULL; cfg->uri = "ip:192.168.2.1"; cfg->tool_dir = NULL; cfg->out_prefix = NULL; cfg->gain_mode = "slow_attack";
    cfg->rx_mode = "auto"; cfg->rx_combine = "max";
    cfg->gain_db = 30; cfg->use_gain_db = false; cfg->no_group = false; cfg->dry_run = false; cfg->sort_snr = false;
    cfg->threshold_override = 0.0; cfg->has_threshold_override = false; cfg->avg_override = 0; cfg->has_avg_override = false;
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) { print_usage(argv[0]); exit(0); }
        else if (strcmp(arg, "--band") == 0) { if (++i >= argc) { fprintf(stderr, "ERROR: --band requires a value\n"); return false; } cfg->band = argv[i]; }
        else if (strcmp(arg, "--uri") == 0) { if (++i >= argc) { fprintf(stderr, "ERROR: --uri requires a value\n"); return false; } cfg->uri = argv[i]; }
        else if (strcmp(arg, "--tool-dir") == 0) { if (++i >= argc) { fprintf(stderr, "ERROR: --tool-dir requires a directory\n"); return false; } cfg->tool_dir = argv[i]; }
        else if (strcmp(arg, "--out-prefix") == 0) { if (++i >= argc) { fprintf(stderr, "ERROR: --out-prefix requires a value\n"); return false; } cfg->out_prefix = argv[i]; }
        else if (strcmp(arg, "--threshold-db") == 0) { if (++i >= argc || !parse_double_value(argv[i], &cfg->threshold_override)) { fprintf(stderr, "ERROR: --threshold-db requires a numeric value\n"); return false; } cfg->has_threshold_override = true; }
        else if (strcmp(arg, "--avg") == 0) { if (++i >= argc || !parse_int_value(argv[i], &cfg->avg_override)) { fprintf(stderr, "ERROR: --avg requires an integer count\n"); return false; } cfg->has_avg_override = true; }
        else if (strcmp(arg, "--gain-mode") == 0) { if (++i >= argc) { fprintf(stderr, "ERROR: --gain-mode requires a value\n"); return false; } cfg->gain_mode = argv[i]; cfg->use_gain_db = (strcmp(cfg->gain_mode, "manual") == 0); }
        else if (strcmp(arg, "--gain-db") == 0) { if (++i >= argc || !parse_ll(argv[i], &cfg->gain_db)) { fprintf(stderr, "ERROR: --gain-db requires an integer dB value\n"); return false; } cfg->gain_mode = "manual"; cfg->use_gain_db = true; }
        else if (strcmp(arg, "--rx-mode") == 0) { if (++i >= argc) { fprintf(stderr, "ERROR: --rx-mode requires auto, single, or dual\n"); return false; } if (strcmp(argv[i], "auto") != 0 && strcmp(argv[i], "single") != 0 && strcmp(argv[i], "dual") != 0) { fprintf(stderr, "ERROR: --rx-mode must be auto, single, or dual\n"); return false; } cfg->rx_mode = argv[i]; }
        else if (strcmp(arg, "--rx-combine") == 0) { if (++i >= argc) { fprintf(stderr, "ERROR: --rx-combine requires max, average, or separate\n"); return false; } if (strcmp(argv[i], "max") != 0 && strcmp(argv[i], "average") != 0 && strcmp(argv[i], "separate") != 0) { fprintf(stderr, "ERROR: --rx-combine must be max, average, or separate\n"); return false; } cfg->rx_combine = argv[i]; }
        else if (strcmp(arg, "--sort") == 0) { if (++i >= argc) { fprintf(stderr, "ERROR: --sort requires freq or snr\n"); return false; } if (strcmp(argv[i], "snr") == 0) cfg->sort_snr = true; else if (strcmp(argv[i], "freq") == 0) cfg->sort_snr = false; else { fprintf(stderr, "ERROR: --sort must be freq or snr\n"); return false; } }
        else if (strcmp(arg, "--no-group") == 0) cfg->no_group = true;
        else if (strcmp(arg, "--dry-run") == 0) cfg->dry_run = true;
        else { fprintf(stderr, "ERROR: Unknown option: %s\n", arg); return false; }
    }
    if (!cfg->band) { fprintf(stderr, "ERROR: --band is required\n"); return false; }
    if (cfg->has_avg_override && cfg->avg_override <= 0) { fprintf(stderr, "ERROR: --avg must be greater than zero\n"); return false; }
    return true;
}

static int run_command(const char *label, const char *command, bool dry_run) {
    printf("\n%s command:\n%s\n", label, command);
    if (dry_run) { printf("Dry run enabled, not executing.\n"); return 0; }
    printf("\nRunning %s...\n", label);
    int ret = system(command);
    if (ret != 0) { fprintf(stderr, "ERROR: %s failed with return code %d\n", label, ret); return ret; }
    return 0;
}

int main(int argc, char **argv) {
    app_config_t cfg;
    if (!parse_args(argc, argv, &cfg)) { print_usage(argv[0]); return 1; }
    const band_preset_t *preset = find_preset(cfg.band);
    if (!preset) { fprintf(stderr, "ERROR: Unknown band preset: %s\nRun with --help to list presets.\n", cfg.band); return 1; }
    if (preset->start_hz < PLUTO_PLUS_DEFAULT_MIN_HZ) { fprintf(stderr, "ERROR: preset starts below Pluto+ default minimum %.3f MHz\n", (double)PLUTO_PLUS_DEFAULT_MIN_HZ / 1e6); return 1; }
    char derived_tool_dir[512]; get_default_tool_dir(argv[0], derived_tool_dir, sizeof(derived_tool_dir));
    const char *tool_dir = cfg.tool_dir ? cfg.tool_dir : derived_tool_dir;
    const char *prefix = cfg.out_prefix ? cfg.out_prefix : preset->id;
    double threshold_db = cfg.has_threshold_override ? cfg.threshold_override : preset->threshold_db;
    int averages = cfg.has_avg_override ? cfg.avg_override : preset->averages;
    char raw_csv[512]; char grouped_csv[512];
    snprintf(raw_csv, sizeof(raw_csv), "%s_raw.csv", prefix);
    snprintf(grouped_csv, sizeof(grouped_csv), "%s_grouped.csv", prefix);
    printf("\nPluto+ Preset Band Scanner\n--------------------------\n");
    printf("Device assumption: Pluto+ RX minimum %.3f MHz\n", (double)PLUTO_PLUS_DEFAULT_MIN_HZ / 1e6);
    printf("Band:             %s\nDescription:      %s\nURI:              %s\nTool directory:   %s\n", preset->id, preset->description, cfg.uri, tool_dir);
    printf("Start:            %lld Hz\nStop:             %lld Hz\nStep:             %lld Hz\nSample rate:      %lld Hz\nBandwidth:        %lld Hz\n", preset->start_hz, preset->stop_hz, preset->step_hz, preset->rate_hz, preset->bw_hz);
    printf("FFT size:         %d\nAverages:         %d\nThreshold:        %.2f dB\nMerge spacing:    %.1f Hz\nDC exclude:       %.1f Hz\nGain mode:        %s\nRX mode:          %s\nRX combine:       %s\n", preset->fft_size, averages, threshold_db, preset->merge_hz, preset->dc_exclude_hz, cfg.gain_mode, cfg.rx_mode, cfg.rx_combine);
    if (cfg.use_gain_db) printf("Manual gain:      %lld dB\n", cfg.gain_db);
    printf("Raw CSV:          %s\n", raw_csv); if (!cfg.no_group) printf("Grouped CSV:      %s\n", grouped_csv);
    char sweep_cmd[4096];
    if (cfg.use_gain_db) {
        snprintf(sweep_cmd, sizeof(sweep_cmd), "call \"%s\\pluto_sweep_scanner.exe\" --uri \"%s\" --start %lld --stop %lld --step %lld --rate %lld --bw %lld --fft %d --avg %d --threshold-db %.3f --dc-exclude-hz %.3f --gain-mode manual --gain-db %lld --rx-mode \"%s\" --rx-combine \"%s\" --csv \"%s\"", tool_dir, cfg.uri, preset->start_hz, preset->stop_hz, preset->step_hz, preset->rate_hz, preset->bw_hz, preset->fft_size, averages, threshold_db, preset->dc_exclude_hz, cfg.gain_db, cfg.rx_mode, cfg.rx_combine, raw_csv);
    } else {
        snprintf(sweep_cmd, sizeof(sweep_cmd), "call \"%s\\pluto_sweep_scanner.exe\" --uri \"%s\" --start %lld --stop %lld --step %lld --rate %lld --bw %lld --fft %d --avg %d --threshold-db %.3f --dc-exclude-hz %.3f --gain-mode \"%s\" --rx-mode \"%s\" --rx-combine \"%s\" --csv \"%s\"", tool_dir, cfg.uri, preset->start_hz, preset->stop_hz, preset->step_hz, preset->rate_hz, preset->bw_hz, preset->fft_size, averages, threshold_db, preset->dc_exclude_hz, cfg.gain_mode, cfg.rx_mode, cfg.rx_combine, raw_csv);
    }
    int ret = run_command("Sweep", sweep_cmd, cfg.dry_run); if (ret != 0) return 1;
    if (cfg.no_group) { printf("\nDone. Raw CSV written: %s\n", raw_csv); return 0; }
    char group_cmd[4096];
    snprintf(group_cmd, sizeof(group_cmd), "call \"%s\\pluto_scan_group.exe\" --in \"%s\" --out \"%s\" --merge-hz %.3f --min-snr %.3f --sort %s", tool_dir, raw_csv, grouped_csv, preset->merge_hz, threshold_db, cfg.sort_snr ? "snr" : "freq");
    ret = run_command("Group", group_cmd, cfg.dry_run); if (ret != 0) return 1;
    printf("\nBand scan complete.\nRaw CSV:     %s\nGrouped CSV: %s\n", raw_csv, grouped_csv);
    return 0;
}
