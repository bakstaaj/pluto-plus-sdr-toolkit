/*
 * pluto_dual_rx_power_scan.c
 *
 * Simple AD9361/Pluto+ power scanner with optional dual-RX support.
 *
 * Intended project defaults:
 *   --rx-mode auto
 *   --rx-combine max
 *
 * RX mapping for Pluto+ AD9361 / 2R2T:
 *   RX1 = cf-ad9361-lpc voltage0 I + voltage1 Q
 *   RX2 = cf-ad9361-lpc voltage2 I + voltage3 Q
 *
 * Build/link requirements:
 *   libiio headers and library
 *
 * Example:
 *   ./pluto_dual_rx_power_scan.exe \
 *     --uri ip:192.168.2.1 \
 *     --freq-file configs/dual_rx_test_freqs.csv \
 *     --rx-mode auto \
 *     --rx-combine max \
 *     --threshold-dbfs -55 \
 *     --csv dual_rx_power_scan_host.csv
 */

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <signal.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define STRCASECMP _stricmp
static void sleep_ms(unsigned int ms) { Sleep(ms); }
#else
#include <strings.h>
#include <unistd.h>
#define STRCASECMP strcasecmp
static void sleep_ms(unsigned int ms) { usleep((useconds_t)ms * 1000U); }
#endif

#include <iio.h>

#ifndef ARRAY_LEN
#define ARRAY_LEN(x) (sizeof(x) / sizeof((x)[0]))
#endif

typedef enum {
    RX_MODE_AUTO = 0,
    RX_MODE_SINGLE,
    RX_MODE_DUAL
} rx_mode_t;

typedef enum {
    RX_COMBINE_MAX = 0,
    RX_COMBINE_AVERAGE,
    RX_COMBINE_SEPARATE
} rx_combine_t;

typedef struct {
    long long hz;
    char label[96];
} freq_entry_t;

typedef struct {
    freq_entry_t *items;
    size_t count;
    size_t cap;
} freq_list_t;

typedef struct {
    const char *uri;
    const char *freq_file;
    const char *csv_path;

    long long start_hz;
    long long stop_hz;
    long long step_hz;
    bool use_range;

    long long sample_rate;
    long long bandwidth;
    size_t sample_count;
    unsigned int settle_ms;

    double threshold_dbfs;

    rx_mode_t rx_mode;
    rx_combine_t rx_combine;

    bool verbose;
} options_t;

typedef struct {
    double rx1_dbfs;
    double rx2_dbfs;
    double combined_dbfs;
    bool rx1_valid;
    bool rx2_valid;
} power_result_t;

static volatile sig_atomic_t g_stop = 0;

static void on_signal(int sig) {
    (void)sig;
    g_stop = 1;
}

static void usage(const char *prog) {
    printf(
        "Usage:\n"
        "  %s --uri ip:192.168.2.1 --freq-file configs/dual_rx_test_freqs.csv [options]\n"
        "  %s --uri ip:192.168.2.1 --start 144000000 --stop 148000000 --step 25000 [options]\n"
        "\n"
        "Options:\n"
        "  --uri <uri>                 IIO URI. Default: ip:192.168.2.1\n"
        "  --freq-file <csv>           CSV/text frequency list. First column is frequency.\n"
        "                              Values below 1,000,000 are treated as MHz.\n"
        "  --start <hz>                Start frequency for range scan.\n"
        "  --stop <hz>                 Stop frequency for range scan.\n"
        "  --step <hz>                 Step size for range scan.\n"
        "  --rate <hz>                 RX sample rate. Default: 960000\n"
        "  --bw <hz>                   RF bandwidth. Default: 1000000\n"
        "  --samples <n>               Samples per frequency. Default: 32768\n"
        "  --settle-ms <n>             Tuning settle delay. Default: 120\n"
        "  --threshold-dbfs <db>       Active threshold. Default: -55\n"
        "  --rx-mode auto|single|dual  Default: auto\n"
        "  --rx-combine max|average|separate\n"
        "                              Default: max\n"
        "  --csv <path>                Output CSV. Default: dual_rx_power_scan.csv\n"
        "  --verbose                   Print per-frequency details.\n"
        "  --help                      Show this help.\n",
        prog, prog);
}

static bool parse_ll(const char *s, long long *out) {
    char *end = NULL;
    errno = 0;
    long long v = strtoll(s, &end, 10);
    if (errno || end == s || *end != '\0') {
        return false;
    }
    *out = v;
    return true;
}

static bool parse_double(const char *s, double *out) {
    char *end = NULL;
    errno = 0;
    double v = strtod(s, &end);
    if (errno || end == s || *end != '\0' || !isfinite(v)) {
        return false;
    }
    *out = v;
    return true;
}

static bool parse_rx_mode(const char *s, rx_mode_t *out) {
    if (STRCASECMP(s, "auto") == 0) {
        *out = RX_MODE_AUTO;
        return true;
    }
    if (STRCASECMP(s, "single") == 0) {
        *out = RX_MODE_SINGLE;
        return true;
    }
    if (STRCASECMP(s, "dual") == 0) {
        *out = RX_MODE_DUAL;
        return true;
    }
    return false;
}

static bool parse_rx_combine(const char *s, rx_combine_t *out) {
    if (STRCASECMP(s, "max") == 0) {
        *out = RX_COMBINE_MAX;
        return true;
    }
    if (STRCASECMP(s, "average") == 0) {
        *out = RX_COMBINE_AVERAGE;
        return true;
    }
    if (STRCASECMP(s, "separate") == 0) {
        *out = RX_COMBINE_SEPARATE;
        return true;
    }
    return false;
}

static const char *rx_mode_name(rx_mode_t m) {
    switch (m) {
    case RX_MODE_AUTO:
        return "auto";
    case RX_MODE_SINGLE:
        return "single";
    case RX_MODE_DUAL:
        return "dual";
    default:
        return "unknown";
    }
}

static const char *rx_combine_name(rx_combine_t c) {
    switch (c) {
    case RX_COMBINE_MAX:
        return "max";
    case RX_COMBINE_AVERAGE:
        return "average";
    case RX_COMBINE_SEPARATE:
        return "separate";
    default:
        return "unknown";
    }
}

static void options_default(options_t *opt) {
    memset(opt, 0, sizeof(*opt));
    opt->uri = "ip:192.168.2.1";
    opt->csv_path = "dual_rx_power_scan.csv";
    opt->sample_rate = 960000;
    opt->bandwidth = 1000000;
    opt->sample_count = 32768;
    opt->settle_ms = 120;
    opt->threshold_dbfs = -55.0;
    opt->rx_mode = RX_MODE_AUTO;
    opt->rx_combine = RX_COMBINE_MAX;
}

static bool require_arg(int argc, char **argv, int *i) {
    if (*i + 1 >= argc) {
        fprintf(stderr, "Missing value for %s\n", argv[*i]);
        return false;
    }
    (*i)++;
    return true;
}

static bool parse_args(int argc, char **argv, options_t *opt) {
    options_default(opt);

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];

        if (strcmp(a, "--help") == 0 || strcmp(a, "-h") == 0) {
            usage(argv[0]);
            exit(0);
        } else if (strcmp(a, "--uri") == 0) {
            if (!require_arg(argc, argv, &i)) return false;
            opt->uri = argv[i];
        } else if (strcmp(a, "--freq-file") == 0) {
            if (!require_arg(argc, argv, &i)) return false;
            opt->freq_file = argv[i];
        } else if (strcmp(a, "--csv") == 0) {
            if (!require_arg(argc, argv, &i)) return false;
            opt->csv_path = argv[i];
        } else if (strcmp(a, "--start") == 0) {
            if (!require_arg(argc, argv, &i)) return false;
            if (!parse_ll(argv[i], &opt->start_hz)) return false;
            opt->use_range = true;
        } else if (strcmp(a, "--stop") == 0) {
            if (!require_arg(argc, argv, &i)) return false;
            if (!parse_ll(argv[i], &opt->stop_hz)) return false;
            opt->use_range = true;
        } else if (strcmp(a, "--step") == 0) {
            if (!require_arg(argc, argv, &i)) return false;
            if (!parse_ll(argv[i], &opt->step_hz)) return false;
            opt->use_range = true;
        } else if (strcmp(a, "--rate") == 0) {
            if (!require_arg(argc, argv, &i)) return false;
            if (!parse_ll(argv[i], &opt->sample_rate)) return false;
        } else if (strcmp(a, "--bw") == 0) {
            if (!require_arg(argc, argv, &i)) return false;
            if (!parse_ll(argv[i], &opt->bandwidth)) return false;
        } else if (strcmp(a, "--samples") == 0) {
            long long tmp = 0;
            if (!require_arg(argc, argv, &i)) return false;
            if (!parse_ll(argv[i], &tmp) || tmp <= 0) return false;
            opt->sample_count = (size_t)tmp;
        } else if (strcmp(a, "--settle-ms") == 0) {
            long long tmp = 0;
            if (!require_arg(argc, argv, &i)) return false;
            if (!parse_ll(argv[i], &tmp) || tmp < 0) return false;
            opt->settle_ms = (unsigned int)tmp;
        } else if (strcmp(a, "--threshold-dbfs") == 0) {
            if (!require_arg(argc, argv, &i)) return false;
            if (!parse_double(argv[i], &opt->threshold_dbfs)) return false;
        } else if (strcmp(a, "--rx-mode") == 0) {
            if (!require_arg(argc, argv, &i)) return false;
            if (!parse_rx_mode(argv[i], &opt->rx_mode)) {
                fprintf(stderr, "Invalid --rx-mode: %s\n", argv[i]);
                return false;
            }
        } else if (strcmp(a, "--rx-combine") == 0) {
            if (!require_arg(argc, argv, &i)) return false;
            if (!parse_rx_combine(argv[i], &opt->rx_combine)) {
                fprintf(stderr, "Invalid --rx-combine: %s\n", argv[i]);
                return false;
            }
        } else if (strcmp(a, "--verbose") == 0) {
            opt->verbose = true;
        } else {
            fprintf(stderr, "Unknown option: %s\n", a);
            return false;
        }
    }

    if (!opt->freq_file && !opt->use_range) {
        fprintf(stderr, "Provide either --freq-file or --start/--stop/--step.\n");
        return false;
    }

    if (opt->use_range) {
        if (opt->start_hz <= 0 || opt->stop_hz <= 0 || opt->step_hz <= 0) {
            fprintf(stderr, "Range scan requires positive --start, --stop, and --step.\n");
            return false;
        }
        if (opt->stop_hz < opt->start_hz) {
            fprintf(stderr, "--stop must be greater than or equal to --start.\n");
            return false;
        }
    }

    return true;
}

static void freq_list_free(freq_list_t *list) {
    free(list->items);
    memset(list, 0, sizeof(*list));
}

static bool freq_list_append(freq_list_t *list, long long hz, const char *label) {
    if (list->count == list->cap) {
        size_t new_cap = list->cap ? list->cap * 2 : 64;
        freq_entry_t *p = (freq_entry_t *)realloc(list->items, new_cap * sizeof(freq_entry_t));
        if (!p) {
            return false;
        }
        list->items = p;
        list->cap = new_cap;
    }
    list->items[list->count].hz = hz;
    if (label && *label) {
        snprintf(list->items[list->count].label, sizeof(list->items[list->count].label), "%s", label);
    } else {
        list->items[list->count].label[0] = '\0';
    }
    list->count++;
    return true;
}

static char *trim(char *s) {
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
    char *e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) {
        *--e = '\0';
    }
    return s;
}

static bool load_freq_file(const char *path, freq_list_t *list) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        perror(path);
        return false;
    }

    char line[512];
    unsigned int lineno = 0;
    while (fgets(line, sizeof(line), fp)) {
        lineno++;
        char *s = trim(line);
        if (!*s || *s == '#') continue;

        char *first = s;
        char *comma = strchr(s, ',');
        char *label = NULL;
        if (comma) {
            *comma = '\0';
            label = trim(comma + 1);
        }

        first = trim(first);

        char *end = NULL;
        errno = 0;
        double value = strtod(first, &end);
        if (errno || end == first) {
            /* Allow a header row like frequency_hz,label. */
            continue;
        }

        long long hz = 0;
        if (value > 0.0 && value < 1000000.0) {
            hz = (long long)llround(value * 1000000.0);
        } else {
            hz = (long long)llround(value);
        }

        if (hz <= 0) {
            fprintf(stderr, "Ignoring invalid frequency on line %u: %s\n", lineno, first);
            continue;
        }

        if (!freq_list_append(list, hz, label)) {
            fclose(fp);
            return false;
        }
    }

    fclose(fp);
    return true;
}

static bool load_range(const options_t *opt, freq_list_t *list) {
    for (long long f = opt->start_hz; f <= opt->stop_hz; f += opt->step_hz) {
        if (!freq_list_append(list, f, "range")) {
            return false;
        }
        if (LLONG_MAX - f < opt->step_hz) {
            break;
        }
    }
    return true;
}

static struct iio_channel *find_chan(struct iio_device *dev, const char *name, bool output) {
    return iio_device_find_channel(dev, name, output);
}

static int write_chan_ll(struct iio_channel *ch, const char *attr, long long value, bool warn) {
    if (!ch) return -1;
    int ret = iio_channel_attr_write_longlong(ch, attr, value);
    if (ret < 0 && warn) {
        fprintf(stderr, "Warning: failed to write %s=%lld on channel: %d\n", attr, value, ret);
    }
    return ret;
}

static int write_chan_str(struct iio_channel *ch, const char *attr, const char *value, bool warn) {
    if (!ch) return -1;
    int ret = iio_channel_attr_write(ch, attr, value);
    if (ret < 0 && warn) {
        fprintf(stderr, "Warning: failed to write %s=%s on channel: %d\n", attr, value, ret);
    }
    return ret;
}

static bool configure_phy(struct iio_context *ctx, const options_t *opt, bool use_dual) {
    struct iio_device *phy = iio_context_find_device(ctx, "ad9361-phy");
    if (!phy) {
        fprintf(stderr, "Could not find ad9361-phy.\n");
        return false;
    }

    struct iio_channel *rx_lo = find_chan(phy, "altvoltage0", true);
    struct iio_channel *rx0 = find_chan(phy, "voltage0", false);
    struct iio_channel *rx1 = find_chan(phy, "voltage1", false);

    if (!rx_lo || !rx0) {
        fprintf(stderr, "Could not find required AD9361 PHY channels.\n");
        return false;
    }

    write_chan_ll(rx0, "sampling_frequency", opt->sample_rate, true);
    write_chan_ll(rx0, "rf_bandwidth", opt->bandwidth, true);
    write_chan_str(rx0, "gain_control_mode", "slow_attack", false);

    if (use_dual && rx1) {
        write_chan_ll(rx1, "sampling_frequency", opt->sample_rate, false);
        write_chan_ll(rx1, "rf_bandwidth", opt->bandwidth, false);
        write_chan_str(rx1, "gain_control_mode", "slow_attack", false);
    }

    return true;
}

static bool tune_frequency(struct iio_context *ctx, long long hz) {
    struct iio_device *phy = iio_context_find_device(ctx, "ad9361-phy");
    if (!phy) {
        fprintf(stderr, "Could not find ad9361-phy.\n");
        return false;
    }

    struct iio_channel *rx_lo = find_chan(phy, "altvoltage0", true);
    if (!rx_lo) {
        fprintf(stderr, "Could not find RX_LO altvoltage0.\n");
        return false;
    }

    int ret = write_chan_ll(rx_lo, "frequency", hz, true);
    return ret >= 0;
}

static double power_to_dbfs(double p) {
    if (p <= 0.0 || !isfinite(p)) {
        return -200.0;
    }
    return 10.0 * log10(p);
}

static power_result_t measure_power_once(struct iio_buffer *buf,
                                         struct iio_channel *rx1_i,
                                         struct iio_channel *rx1_q,
                                         struct iio_channel *rx2_i,
                                         struct iio_channel *rx2_q,
                                         bool use_dual,
                                         rx_combine_t combine) {
    power_result_t r;
    memset(&r, 0, sizeof(r));
    r.rx1_dbfs = -200.0;
    r.rx2_dbfs = -200.0;
    r.combined_dbfs = -200.0;
    r.rx1_valid = true;
    r.rx2_valid = use_dual;

    ssize_t nbytes = iio_buffer_refill(buf);
    if (nbytes < 0) {
        fprintf(stderr, "iio_buffer_refill failed: %ld\n", (long)nbytes);
        r.rx1_valid = false;
        r.rx2_valid = false;
        return r;
    }

    char *end = (char *)iio_buffer_end(buf);
    ptrdiff_t step = iio_buffer_step(buf);

    char *rx1i = (char *)iio_buffer_first(buf, rx1_i);
    char *rx1q = (char *)iio_buffer_first(buf, rx1_q);
    char *rx2i = use_dual ? (char *)iio_buffer_first(buf, rx2_i) : NULL;
    char *rx2q = use_dual ? (char *)iio_buffer_first(buf, rx2_q) : NULL;

    if (!rx1i || !rx1q || (use_dual && (!rx2i || !rx2q)) || step <= 0) {
        fprintf(stderr, "Invalid buffer layout.\n");
        r.rx1_valid = false;
        r.rx2_valid = false;
        return r;
    }

    double sum1 = 0.0;
    double sum2 = 0.0;
    size_t count = 0;

    for (; rx1i < end && rx1q < end; rx1i += step, rx1q += step) {
        int16_t i1 = *(int16_t *)rx1i;
        int16_t q1 = *(int16_t *)rx1q;

        double fi1 = (double)i1 / 32768.0;
        double fq1 = (double)q1 / 32768.0;
        sum1 += (fi1 * fi1 + fq1 * fq1) * 0.5;

        if (use_dual && rx2i < end && rx2q < end) {
            int16_t i2 = *(int16_t *)rx2i;
            int16_t q2 = *(int16_t *)rx2q;
            double fi2 = (double)i2 / 32768.0;
            double fq2 = (double)q2 / 32768.0;
            sum2 += (fi2 * fi2 + fq2 * fq2) * 0.5;
            rx2i += step;
            rx2q += step;
        }

        count++;
    }

    if (count == 0) {
        r.rx1_valid = false;
        r.rx2_valid = false;
        return r;
    }

    double p1 = sum1 / (double)count;
    double p2 = use_dual ? (sum2 / (double)count) : 0.0;

    r.rx1_dbfs = power_to_dbfs(p1);
    if (use_dual) {
        r.rx2_dbfs = power_to_dbfs(p2);
    }

    switch (combine) {
    case RX_COMBINE_MAX:
        r.combined_dbfs = use_dual ? ((r.rx1_dbfs > r.rx2_dbfs) ? r.rx1_dbfs : r.rx2_dbfs) : r.rx1_dbfs;
        break;
    case RX_COMBINE_AVERAGE:
        r.combined_dbfs = use_dual ? power_to_dbfs((p1 + p2) * 0.5) : r.rx1_dbfs;
        break;
    case RX_COMBINE_SEPARATE:
        /* Keep a useful active decision: either receiver above threshold. */
        r.combined_dbfs = use_dual ? ((r.rx1_dbfs > r.rx2_dbfs) ? r.rx1_dbfs : r.rx2_dbfs) : r.rx1_dbfs;
        break;
    default:
        r.combined_dbfs = r.rx1_dbfs;
        break;
    }

    return r;
}

int main(int argc, char **argv) {
    options_t opt;
    if (!parse_args(argc, argv, &opt)) {
        usage(argv[0]);
        return 2;
    }

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    freq_list_t freqs;
    memset(&freqs, 0, sizeof(freqs));

    if (opt.freq_file) {
        if (!load_freq_file(opt.freq_file, &freqs)) {
            freq_list_free(&freqs);
            return 1;
        }
    }

    if (opt.use_range) {
        if (!load_range(&opt, &freqs)) {
            freq_list_free(&freqs);
            return 1;
        }
    }

    if (freqs.count == 0) {
        fprintf(stderr, "No frequencies to scan.\n");
        freq_list_free(&freqs);
        return 1;
    }

    printf("Pluto Dual RX Power Scanner\n");
    printf("URI: %s\n", opt.uri);
    printf("Frequencies: %zu\n", freqs.count);
    printf("Requested RX mode: %s\n", rx_mode_name(opt.rx_mode));
    printf("RX combine: %s\n", rx_combine_name(opt.rx_combine));
    printf("Threshold: %.1f dBFS\n", opt.threshold_dbfs);

    struct iio_context *ctx = iio_create_context_from_uri(opt.uri);
    if (!ctx) {
        fprintf(stderr, "Failed to create IIO context for URI: %s\n", opt.uri);
        freq_list_free(&freqs);
        return 1;
    }

    struct iio_device *rxdev = iio_context_find_device(ctx, "cf-ad9361-lpc");
    if (!rxdev) {
        fprintf(stderr, "Could not find cf-ad9361-lpc RX buffer device.\n");
        iio_context_destroy(ctx);
        freq_list_free(&freqs);
        return 1;
    }

    struct iio_channel *rx1_i = find_chan(rxdev, "voltage0", false);
    struct iio_channel *rx1_q = find_chan(rxdev, "voltage1", false);
    struct iio_channel *rx2_i = find_chan(rxdev, "voltage2", false);
    struct iio_channel *rx2_q = find_chan(rxdev, "voltage3", false);

    bool rx1_ok = (rx1_i && rx1_q);
    bool rx2_ok = (rx2_i && rx2_q);

    printf("RX1 channels: voltage0/voltage1 %s\n", rx1_ok ? "OK" : "MISSING");
    printf("RX2 channels: voltage2/voltage3 %s\n", rx2_ok ? "OK" : "MISSING");

    if (!rx1_ok) {
        fprintf(stderr, "RX1 I/Q channels are required.\n");
        iio_context_destroy(ctx);
        freq_list_free(&freqs);
        return 1;
    }

    bool use_dual = false;
    if (opt.rx_mode == RX_MODE_DUAL) {
        if (!rx2_ok) {
            fprintf(stderr, "--rx-mode dual requested, but RX2 channels are missing.\n");
            iio_context_destroy(ctx);
            freq_list_free(&freqs);
            return 1;
        }
        use_dual = true;
    } else if (opt.rx_mode == RX_MODE_AUTO) {
        use_dual = rx2_ok;
    } else {
        use_dual = false;
    }

    printf("Effective RX mode: %s\n", use_dual ? "dual" : "single");
    printf("Dual available: %s\n", rx2_ok ? "yes" : "no");

    if (!configure_phy(ctx, &opt, use_dual)) {
        iio_context_destroy(ctx);
        freq_list_free(&freqs);
        return 1;
    }

    iio_channel_enable(rx1_i);
    iio_channel_enable(rx1_q);
    if (use_dual) {
        iio_channel_enable(rx2_i);
        iio_channel_enable(rx2_q);
    }

    iio_device_set_kernel_buffers_count(rxdev, 1);

    struct iio_buffer *buf = iio_device_create_buffer(rxdev, opt.sample_count, false);
    if (!buf) {
        fprintf(stderr, "Failed to create RX buffer. sample_count=%zu\n", opt.sample_count);
        iio_channel_disable(rx1_i);
        iio_channel_disable(rx1_q);
        if (use_dual) {
            iio_channel_disable(rx2_i);
            iio_channel_disable(rx2_q);
        }
        iio_context_destroy(ctx);
        freq_list_free(&freqs);
        return 1;
    }

    FILE *csv = fopen(opt.csv_path, "w");
    if (!csv) {
        perror(opt.csv_path);
        iio_buffer_destroy(buf);
        iio_context_destroy(ctx);
        freq_list_free(&freqs);
        return 1;
    }

    fprintf(csv, "frequency_hz,label,effective_rx_mode,rx_combine,rx1_dbfs,rx2_dbfs,combined_dbfs,threshold_dbfs,active\n");

    size_t active_count = 0;
    for (size_t idx = 0; idx < freqs.count && !g_stop; idx++) {
        long long f = freqs.items[idx].hz;
        const char *label = freqs.items[idx].label;

        if (!tune_frequency(ctx, f)) {
            fprintf(stderr, "Tune failed for %lld Hz\n", f);
            fprintf(csv, "%lld,%s,%s,%s,,,,%.2f,tune_failed\n",
                    f, label, use_dual ? "dual" : "single",
                    rx_combine_name(opt.rx_combine),
                    opt.threshold_dbfs);
            fflush(csv);
            continue;
        }

        sleep_ms(opt.settle_ms);

        power_result_t p = measure_power_once(buf, rx1_i, rx1_q, rx2_i, rx2_q, use_dual, opt.rx_combine);
        bool active = p.combined_dbfs >= opt.threshold_dbfs;
        if (active) active_count++;

        fprintf(csv, "%lld,%s,%s,%s,%.2f,%.2f,%.2f,%.2f,%s\n",
                f,
                label,
                use_dual ? "dual" : "single",
                rx_combine_name(opt.rx_combine),
                p.rx1_dbfs,
                use_dual ? p.rx2_dbfs : -200.0,
                p.combined_dbfs,
                opt.threshold_dbfs,
                active ? "yes" : "no");
        fflush(csv);

        if (opt.verbose || active) {
            if (use_dual) {
                printf("%10lld Hz  RX1=%7.2f dBFS  RX2=%7.2f dBFS  combined=%7.2f  %s%s\n",
                       f, p.rx1_dbfs, p.rx2_dbfs, p.combined_dbfs,
                       active ? "ACTIVE" : "idle",
                       label && *label ? "  " : "");
            } else {
                printf("%10lld Hz  RX1=%7.2f dBFS  combined=%7.2f  %s%s\n",
                       f, p.rx1_dbfs, p.combined_dbfs,
                       active ? "ACTIVE" : "idle",
                       label && *label ? "  " : "");
            }
        }
    }

    printf("Done. Active frequencies: %zu / %zu\n", active_count, freqs.count);
    printf("CSV: %s\n", opt.csv_path);

    fclose(csv);
    iio_buffer_destroy(buf);
    iio_channel_disable(rx1_i);
    iio_channel_disable(rx1_q);
    if (use_dual) {
        iio_channel_disable(rx2_i);
        iio_channel_disable(rx2_q);
    }
    iio_context_destroy(ctx);
    freq_list_free(&freqs);

    return g_stop ? 130 : 0;
}
