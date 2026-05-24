#include <iio.h>
#include <ad9361.h>

#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*
   pluto_dual_rx_probe.c

   Pluto+ dual-RX validation tool.

   Validates whether cf-ad9361-lpc exposes:
     RX1 I/Q: voltage0 + voltage1
     RX2 I/Q: voltage2 + voltage3

   It captures IQ samples and reports RX1/RX2 average power independently.

   Build fix:
     - Uses localtime_s on Windows/UCRT.
     - Uses localtime_r on POSIX targets.
     - Avoids one-line if/statement patterns that cause misleading-indentation warnings.

   Examples:
     ./pluto_dual_rx_probe.exe --uri ip:192.168.2.1 --freq 146520000 --seconds 2
     ./pluto_dual_rx_probe.exe --uri ip:localhost --freq 100000000 --rx-mode dual --csv /tmp/pluto_ham_scan/sessions/dual_rx_probe.csv
*/

#define DEFAULT_URI "ip:192.168.2.1"

typedef struct {
    const char *uri;
    const char *rx_mode;
    const char *csv_path;

    long long freq_hz;
    long long rate_hz;
    long long bw_hz;

    const char *gain_mode;
    long long gain_db;
    bool use_manual_gain;

    int seconds;
    int buffer_samples;
    int max_refills;
    bool verbose;
} app_config_t;

typedef struct {
    double sum_power;
    double peak_mag;
    unsigned long long count;
} power_stats_t;

static void print_usage(const char *prog)
{
    printf("\n");
    printf("Pluto+ Dual RX Probe\n");
    printf("\n");
    printf("Usage:\n");
    printf("  %s [options]\n", prog);
    printf("\n");
    printf("Options:\n");
    printf("  --uri <uri>              IIO URI, default " DEFAULT_URI "\n");
    printf("  --freq <hz>              RX LO frequency, default 146520000\n");
    printf("  --rate <hz>              Baseband sample rate, default 960000\n");
    printf("  --bw <hz>                RF bandwidth, default 1000000\n");
    printf("  --rx-mode <mode>         auto, single, dual. default auto\n");
    printf("  --seconds <n>            Capture seconds, default 2\n");
    printf("  --buffer-samples <n>     IIO buffer samples, default 8192\n");
    printf("  --max-refills <n>        Override refill count instead of seconds\n");
    printf("  --gain-mode <mode>       slow_attack, fast_attack, manual. default slow_attack\n");
    printf("  --gain-db <db>           Manual RX gain dB, implies --gain-mode manual\n");
    printf("  --csv <file>             Append one CSV result row\n");
    printf("  --verbose                Print refill progress\n");
    printf("  --help                   Show this help\n");
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

static bool parse_args(int argc, char **argv, app_config_t *cfg)
{
    cfg->uri = DEFAULT_URI;
    cfg->rx_mode = "auto";
    cfg->csv_path = NULL;

    cfg->freq_hz = 146520000LL;
    cfg->rate_hz = 960000LL;
    cfg->bw_hz = 1000000LL;

    cfg->gain_mode = "slow_attack";
    cfg->gain_db = 40;
    cfg->use_manual_gain = false;

    cfg->seconds = 2;
    cfg->buffer_samples = 8192;
    cfg->max_refills = 0;
    cfg->verbose = false;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];

        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            print_usage(argv[0]);
            exit(0);
        } else if (strcmp(arg, "--uri") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "ERROR: --uri requires value\n");
                return false;
            }
            cfg->uri = argv[i];
        } else if (strcmp(arg, "--freq") == 0) {
            if (++i >= argc || !parse_ll(argv[i], &cfg->freq_hz)) {
                fprintf(stderr, "ERROR: --freq requires integer Hz\n");
                return false;
            }
        } else if (strcmp(arg, "--rate") == 0) {
            if (++i >= argc || !parse_ll(argv[i], &cfg->rate_hz)) {
                fprintf(stderr, "ERROR: --rate requires integer Hz\n");
                return false;
            }
        } else if (strcmp(arg, "--bw") == 0) {
            if (++i >= argc || !parse_ll(argv[i], &cfg->bw_hz)) {
                fprintf(stderr, "ERROR: --bw requires integer Hz\n");
                return false;
            }
        } else if (strcmp(arg, "--rx-mode") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "ERROR: --rx-mode requires value\n");
                return false;
            }
            cfg->rx_mode = argv[i];
        } else if (strcmp(arg, "--seconds") == 0) {
            if (++i >= argc || !parse_int_value(argv[i], &cfg->seconds)) {
                fprintf(stderr, "ERROR: --seconds requires integer\n");
                return false;
            }
        } else if (strcmp(arg, "--buffer-samples") == 0) {
            if (++i >= argc || !parse_int_value(argv[i], &cfg->buffer_samples)) {
                fprintf(stderr, "ERROR: --buffer-samples requires integer\n");
                return false;
            }
        } else if (strcmp(arg, "--max-refills") == 0) {
            if (++i >= argc || !parse_int_value(argv[i], &cfg->max_refills)) {
                fprintf(stderr, "ERROR: --max-refills requires integer\n");
                return false;
            }
        } else if (strcmp(arg, "--gain-mode") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "ERROR: --gain-mode requires value\n");
                return false;
            }
            cfg->gain_mode = argv[i];
        } else if (strcmp(arg, "--gain-db") == 0) {
            if (++i >= argc || !parse_ll(argv[i], &cfg->gain_db)) {
                fprintf(stderr, "ERROR: --gain-db requires integer dB\n");
                return false;
            }
            cfg->gain_mode = "manual";
            cfg->use_manual_gain = true;
        } else if (strcmp(arg, "--csv") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "ERROR: --csv requires file\n");
                return false;
            }
            cfg->csv_path = argv[i];
        } else if (strcmp(arg, "--verbose") == 0) {
            cfg->verbose = true;
        } else {
            fprintf(stderr, "ERROR: Unknown option: %s\n", arg);
            return false;
        }
    }

    if (strcmp(cfg->rx_mode, "auto") != 0 &&
        strcmp(cfg->rx_mode, "single") != 0 &&
        strcmp(cfg->rx_mode, "dual") != 0) {
        fprintf(stderr, "ERROR: --rx-mode must be auto, single, or dual\n");
        return false;
    }

    if (cfg->freq_hz <= 0 ||
        cfg->rate_hz <= 0 ||
        cfg->bw_hz <= 0 ||
        cfg->seconds <= 0 ||
        cfg->buffer_samples <= 0) {
        fprintf(stderr, "ERROR: freq/rate/bw/seconds/buffer-samples must be positive\n");
        return false;
    }

    return true;
}

static int write_attr_ll(struct iio_channel *ch, const char *attr, long long value)
{
    if (!ch) {
        return -1;
    }

    int ret = iio_channel_attr_write_longlong(ch, attr, value);

    if (ret < 0) {
        fprintf(stderr, "WARNING: could not set %s=%lld ret=%d\n", attr, value, ret);
    }

    return ret;
}

static int write_attr_str(struct iio_channel *ch, const char *attr, const char *value)
{
    if (!ch) {
        return -1;
    }

    int ret = iio_channel_attr_write(ch, attr, value);

    if (ret < 0) {
        fprintf(stderr, "WARNING: could not set %s=%s ret=%d\n", attr, value, ret);
    }

    return ret;
}

static double dbfs_from_mean_power(double p)
{
    if (p <= 1e-20) {
        return -200.0;
    }

    return 10.0 * log10(p);
}

static void stats_update(power_stats_t *s, int16_t i_raw, int16_t q_raw)
{
    /*
       cf-ad9361-lpc reports le:S12/16>>0 on the tested Pluto+ firmware.
       Full-scale signed 12-bit is approximately +/-2048.
    */
    double i = (double)i_raw / 2048.0;
    double q = (double)q_raw / 2048.0;
    double p = i * i + q * q;
    double mag = sqrt(p);

    s->sum_power += p;

    if (mag > s->peak_mag) {
        s->peak_mag = mag;
    }

    s->count++;
}

static void safe_localtime(const time_t *t, struct tm *tm_value)
{
#if defined(_WIN32)
    localtime_s(tm_value, t);
#else
    localtime_r(t, tm_value);
#endif
}

static void make_timestamp(char *out, size_t out_size)
{
    time_t now = time(NULL);
    struct tm tm_value;

    memset(&tm_value, 0, sizeof(tm_value));
    safe_localtime(&now, &tm_value);

    strftime(out, out_size, "%Y-%m-%d %H:%M:%S", &tm_value);
}

static bool file_exists(const char *path)
{
    FILE *fp = fopen(path, "rb");

    if (!fp) {
        return false;
    }

    fclose(fp);
    return true;
}

static void append_csv(const app_config_t *cfg,
                       const char *effective_rx_mode,
                       bool dual_available,
                       const power_stats_t *rx1,
                       const power_stats_t *rx2)
{
    if (!cfg->csv_path) {
        return;
    }

    bool exists = file_exists(cfg->csv_path);
    FILE *fp = fopen(cfg->csv_path, "a");

    if (!fp) {
        fprintf(stderr, "WARNING: could not open CSV: %s\n", cfg->csv_path);
        return;
    }

    if (!exists) {
        fprintf(fp,
                "timestamp,uri,freq_hz,rate_hz,bw_hz,requested_rx_mode,effective_rx_mode,"
                "dual_available,rx1_samples,rx1_avg_dbfs,rx1_peak,"
                "rx2_samples,rx2_avg_dbfs,rx2_peak,rx2_minus_rx1_db\n");
    }

    char ts[64];
    make_timestamp(ts, sizeof(ts));

    double rx1_avg = rx1->count ? dbfs_from_mean_power(rx1->sum_power / (double)rx1->count) : -200.0;
    double rx2_avg = rx2->count ? dbfs_from_mean_power(rx2->sum_power / (double)rx2->count) : -200.0;
    double delta = rx2_avg - rx1_avg;

    fprintf(fp,
            "%s,%s,%lld,%lld,%lld,%s,%s,%d,%llu,%.3f,%.6f,%llu,%.3f,%.6f,%.3f\n",
            ts,
            cfg->uri,
            cfg->freq_hz,
            cfg->rate_hz,
            cfg->bw_hz,
            cfg->rx_mode,
            effective_rx_mode,
            dual_available ? 1 : 0,
            rx1->count,
            rx1_avg,
            rx1->peak_mag,
            rx2->count,
            rx2_avg,
            rx2->peak_mag,
            delta);

    fclose(fp);
}

static void cleanup(struct iio_buffer *rxbuf, struct iio_context *ctx)
{
    if (rxbuf) {
        iio_buffer_destroy(rxbuf);
    }

    if (ctx) {
        iio_context_destroy(ctx);
    }
}

int main(int argc, char **argv)
{
    app_config_t cfg;

    if (!parse_args(argc, argv, &cfg)) {
        print_usage(argv[0]);
        return 1;
    }

    printf("\nPluto+ Dual RX Probe\n");
    printf("--------------------\n");
    printf("URI:        %s\n", cfg.uri);
    printf("Frequency:  %lld Hz\n", cfg.freq_hz);
    printf("Rate:       %lld Hz\n", cfg.rate_hz);
    printf("Bandwidth:  %lld Hz\n", cfg.bw_hz);
    printf("RX mode:    %s\n", cfg.rx_mode);
    printf("Seconds:    %d\n", cfg.seconds);
    printf("Buffer:     %d samples\n", cfg.buffer_samples);
    printf("CSV:        %s\n", cfg.csv_path ? cfg.csv_path : "(none)");
    printf("\n");

    struct iio_context *ctx = NULL;
    struct iio_buffer *rxbuf = NULL;

    ctx = iio_create_context_from_uri(cfg.uri);

    if (!ctx) {
        fprintf(stderr, "ERROR: could not open IIO context: %s\n", cfg.uri);
        return 1;
    }

    struct iio_device *phy = iio_context_find_device(ctx, "ad9361-phy");
    struct iio_device *rxdev = iio_context_find_device(ctx, "cf-ad9361-lpc");

    if (!phy || !rxdev) {
        fprintf(stderr, "ERROR: missing ad9361-phy or cf-ad9361-lpc\n");
        cleanup(rxbuf, ctx);
        return 1;
    }

    struct iio_channel *rx0_i = iio_device_find_channel(rxdev, "voltage0", false);
    struct iio_channel *rx0_q = iio_device_find_channel(rxdev, "voltage1", false);
    struct iio_channel *rx1_i = iio_device_find_channel(rxdev, "voltage2", false);
    struct iio_channel *rx1_q = iio_device_find_channel(rxdev, "voltage3", false);

    bool rx1_available = (rx0_i != NULL && rx0_q != NULL);
    bool rx2_available = (rx1_i != NULL && rx1_q != NULL);
    bool dual_available = (rx1_available && rx2_available);

    if (!rx1_available) {
        fprintf(stderr, "ERROR: RX1 voltage0/voltage1 channels are missing\n");
        cleanup(rxbuf, ctx);
        return 1;
    }

    const char *effective_rx_mode = "single";

    if (strcmp(cfg.rx_mode, "dual") == 0) {
        if (!dual_available) {
            fprintf(stderr, "ERROR: --rx-mode dual requested but voltage2/voltage3 are missing\n");
            cleanup(rxbuf, ctx);
            return 1;
        }

        effective_rx_mode = "dual";
    } else if (strcmp(cfg.rx_mode, "auto") == 0) {
        effective_rx_mode = dual_available ? "dual" : "single";
    } else {
        effective_rx_mode = "single";
    }

    printf("RX1 channels: voltage0/voltage1 %s\n", rx1_available ? "OK" : "MISSING");
    printf("RX2 channels: voltage2/voltage3 %s\n", rx2_available ? "OK" : "MISSING");
    printf("Effective RX mode: %s\n", effective_rx_mode);
    printf("\n");

    struct iio_channel *phy_rx0 = iio_device_find_channel(phy, "voltage0", false);
    struct iio_channel *rx_lo = iio_device_find_channel(phy, "altvoltage0", true);

    if (!phy_rx0 || !rx_lo) {
        fprintf(stderr, "ERROR: missing phy voltage0 or RX_LO channel\n");
        cleanup(rxbuf, ctx);
        return 1;
    }

    int ret = ad9361_set_bb_rate(phy, (unsigned long)cfg.rate_hz);

    if (ret < 0) {
        fprintf(stderr, "ERROR: ad9361_set_bb_rate(%lld) failed ret=%d\n", cfg.rate_hz, ret);
        cleanup(rxbuf, ctx);
        return 1;
    }

    write_attr_ll(phy_rx0, "rf_bandwidth", cfg.bw_hz);
    write_attr_str(phy_rx0, "gain_control_mode", cfg.gain_mode);

    if (cfg.use_manual_gain) {
        write_attr_ll(phy_rx0, "hardwaregain", cfg.gain_db);
    }

    write_attr_ll(rx_lo, "frequency", cfg.freq_hz);

    iio_channel_enable(rx0_i);
    iio_channel_enable(rx0_q);

    if (strcmp(effective_rx_mode, "dual") == 0) {
        iio_channel_enable(rx1_i);
        iio_channel_enable(rx1_q);
    }

    rxbuf = iio_device_create_buffer(rxdev, (size_t)cfg.buffer_samples, false);

    if (!rxbuf) {
        fprintf(stderr, "ERROR: could not create RX buffer\n");
        cleanup(rxbuf, ctx);
        return 1;
    }

    int refills = cfg.max_refills;

    if (refills <= 0) {
        double samples_needed = (double)cfg.seconds * (double)cfg.rate_hz;
        refills = (int)ceil(samples_needed / (double)cfg.buffer_samples);

        if (refills < 1) {
            refills = 1;
        }
    }

    printf("Capturing %d refill(s)...\n", refills);

    power_stats_t rx1 = {0};
    power_stats_t rx2 = {0};

    for (int r = 0; r < refills; r++) {
        ssize_t nbytes = iio_buffer_refill(rxbuf);

        if (nbytes < 0) {
            fprintf(stderr, "ERROR: RX buffer refill failed: %zd\n", nbytes);
            cleanup(rxbuf, ctx);
            return 1;
        }

        char *p0 = (char *)iio_buffer_first(rxbuf, rx0_i);
        char *p1 = (char *)iio_buffer_first(rxbuf, rx0_q);
        char *end = (char *)iio_buffer_end(rxbuf);
        ptrdiff_t step = iio_buffer_step(rxbuf);

        char *p2 = NULL;
        char *p3 = NULL;

        if (strcmp(effective_rx_mode, "dual") == 0) {
            p2 = (char *)iio_buffer_first(rxbuf, rx1_i);
            p3 = (char *)iio_buffer_first(rxbuf, rx1_q);
        }

        for (; p0 < end && p1 < end; p0 += step, p1 += step) {
            int16_t i0 = 0;
            int16_t q0 = 0;

            memcpy(&i0, p0, sizeof(int16_t));
            memcpy(&q0, p1, sizeof(int16_t));

            stats_update(&rx1, i0, q0);

            if (strcmp(effective_rx_mode, "dual") == 0 &&
                p2 != NULL &&
                p3 != NULL &&
                p2 < end &&
                p3 < end) {
                int16_t i1 = 0;
                int16_t q1 = 0;

                memcpy(&i1, p2, sizeof(int16_t));
                memcpy(&q1, p3, sizeof(int16_t));

                stats_update(&rx2, i1, q1);

                p2 += step;
                p3 += step;
            }
        }

        if (cfg.verbose) {
            printf("  refill %d/%d complete\n", r + 1, refills);
        }
    }

    double rx1_avg = rx1.count ? dbfs_from_mean_power(rx1.sum_power / (double)rx1.count) : -200.0;
    double rx2_avg = rx2.count ? dbfs_from_mean_power(rx2.sum_power / (double)rx2.count) : -200.0;
    double delta = rx2_avg - rx1_avg;

    printf("\nResults\n");
    printf("-------\n");
    printf("Effective mode:       %s\n", effective_rx_mode);
    printf("Dual available:       %s\n", dual_available ? "yes" : "no");
    printf("RX1 samples:          %llu\n", rx1.count);
    printf("RX1 average power:    %.2f dBFS\n", rx1_avg);
    printf("RX1 peak magnitude:   %.6f\n", rx1.peak_mag);

    if (strcmp(effective_rx_mode, "dual") == 0) {
        printf("RX2 samples:          %llu\n", rx2.count);
        printf("RX2 average power:    %.2f dBFS\n", rx2_avg);
        printf("RX2 peak magnitude:   %.6f\n", rx2.peak_mag);
        printf("RX2 - RX1 delta:      %.2f dB\n", delta);
    }

    append_csv(&cfg, effective_rx_mode, dual_available, &rx1, &rx2);

    if (cfg.csv_path) {
        printf("CSV appended:         %s\n", cfg.csv_path);
    }

    cleanup(rxbuf, ctx);
    return 0;
}
