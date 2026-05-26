#include <iio.h>
#include <ad9361.h>
#include <fftw3.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <errno.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define PI_CONST 3.14159265358979323846

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
    const char *uri;
    const char *csv_file;

    long long start_hz;
    long long stop_hz;
    long long step_hz;
    long long sample_rate_hz;
    long long bandwidth_hz;
    long long manual_gain_db;

    size_t fft_size;
    size_t buffer_samples;

    int averages;
    int top_peaks;
    int exclude_bins;
    int settle_ms;

    double threshold_db;
    double dc_exclude_hz;

    const char *gain_mode;
    rx_mode_t rx_mode;
    rx_combine_t rx_combine;
    bool use_manual_gain;
    bool write_csv;
} app_config_t;

static void sleep_ms(int ms)
{
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    usleep((useconds_t)ms * 1000);
#endif
}

static void print_usage(const char *prog)
{
    printf("\n");
    printf("Pluto+ Sweep Scanner - Pluto+ dual RX capable mode\n");
    printf("\n");
    printf("Usage:\n");
    printf("  %s [options]\n", prog);
    printf("\n");
    printf("Options:\n");
    printf("  --uri <uri>             IIO URI, default ip:192.168.2.1\n");
    printf("  --start <hz>            Start frequency Hz, default 144000000\n");
    printf("  --stop <hz>             Stop frequency Hz, default 148000000\n");
    printf("  --step <hz>             Tuning step Hz, default 500000\n");
    printf("  --rate <hz>             Sample rate Hz, default 1000000\n");
    printf("  --bw <hz>               RF bandwidth Hz, default equals sample rate\n");
    printf("  --fft <size>            FFT size, default 16384\n");
    printf("  --avg <count>           FFT averages per tuning step, default 4\n");
    printf("  --top <count>           Max peaks per tuning step, default 5\n");
    printf("  --threshold-db <db>     Report peaks this many dB above noise, default 12\n");
    printf("  --exclude-bins <count>  Suppress bins near detected peaks, default 20\n");
    printf("  --dc-exclude-hz <hz>    Ignore center/DC region, default 5000\n");
    printf("  --settle-ms <ms>        Delay after tuning, default 100\n");
    printf("  --gain-mode <mode>      slow_attack, fast_attack, manual, default slow_attack\n");
    printf("  --gain-db <db>          Manual RX gain dB, only used with manual gain mode\n");
    printf("  --rx-mode <mode>        auto, single, dual. Default auto\n");
    printf("  --rx-combine <mode>     max, average, separate. Default max\n");
    printf("                          separate keeps CSV-compatible max behavior for sweep output\n");
    printf("  --csv <file>            Write detected peaks to CSV\n");
    printf("  --help                  Show this help\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s --start 144000000 --stop 148000000 --step 500000 --csv two_meter.csv\n", prog);
    printf("  %s --start 118000000 --stop 137000000 --step 500000 --rate 1000000 --csv airband.csv\n", prog);
    printf("  %s --start 420000000 --stop 450000000 --step 1000000 --rate 2000000 --csv uhf.csv\n", prog);
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

static bool parse_size_value(const char *text, size_t *value)
{
    char *end = NULL;
    errno = 0;

    unsigned long long v = strtoull(text, &end, 10);

    if (errno != 0 || end == text || *end != '\0') {
        return false;
    }

    *value = (size_t)v;
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

static bool parse_rx_mode_value(const char *text, rx_mode_t *value)
{
    if (strcmp(text, "auto") == 0) { *value = RX_MODE_AUTO; return true; }
    if (strcmp(text, "single") == 0) { *value = RX_MODE_SINGLE; return true; }
    if (strcmp(text, "dual") == 0) { *value = RX_MODE_DUAL; return true; }
    return false;
}

static bool parse_rx_combine_value(const char *text, rx_combine_t *value)
{
    if (strcmp(text, "max") == 0) { *value = RX_COMBINE_MAX; return true; }
    if (strcmp(text, "average") == 0) { *value = RX_COMBINE_AVERAGE; return true; }
    if (strcmp(text, "separate") == 0) { *value = RX_COMBINE_SEPARATE; return true; }
    return false;
}

static const char *rx_mode_name(rx_mode_t mode)
{
    switch (mode) {
    case RX_MODE_AUTO: return "auto";
    case RX_MODE_SINGLE: return "single";
    case RX_MODE_DUAL: return "dual";
    default: return "unknown";
    }
}

static const char *rx_combine_name(rx_combine_t combine)
{
    switch (combine) {
    case RX_COMBINE_MAX: return "max";
    case RX_COMBINE_AVERAGE: return "average";
    case RX_COMBINE_SEPARATE: return "separate";
    default: return "unknown";
    }
}

static bool is_power_of_two(size_t n)
{
    return n > 0 && ((n & (n - 1)) == 0);
}

static bool parse_args(int argc, char **argv, app_config_t *cfg)
{
    cfg->uri = "ip:192.168.2.1";
    cfg->csv_file = NULL;

    cfg->start_hz = 144000000LL;
    cfg->stop_hz = 148000000LL;
    cfg->step_hz = 500000LL;
    cfg->sample_rate_hz = 1000000LL;
    cfg->bandwidth_hz = 0;
    cfg->manual_gain_db = 30;

    cfg->fft_size = 16384;
    cfg->buffer_samples = 16384;

    cfg->averages = 4;
    cfg->top_peaks = 5;
    cfg->exclude_bins = 20;
    cfg->settle_ms = 100;

    cfg->threshold_db = 12.0;
    cfg->dc_exclude_hz = 5000.0;

    cfg->gain_mode = "slow_attack";
    cfg->rx_mode = RX_MODE_AUTO;
    cfg->rx_combine = RX_COMBINE_MAX;
    cfg->use_manual_gain = false;
    cfg->write_csv = false;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];

        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            print_usage(argv[0]);
            exit(0);
        } else if (strcmp(arg, "--uri") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "ERROR: --uri requires a value\n");
                return false;
            }
            cfg->uri = argv[i];
        } else if (strcmp(arg, "--start") == 0) {
            if (++i >= argc || !parse_ll(argv[i], &cfg->start_hz)) {
                fprintf(stderr, "ERROR: --start requires an integer Hz value\n");
                return false;
            }
        } else if (strcmp(arg, "--stop") == 0) {
            if (++i >= argc || !parse_ll(argv[i], &cfg->stop_hz)) {
                fprintf(stderr, "ERROR: --stop requires an integer Hz value\n");
                return false;
            }
        } else if (strcmp(arg, "--step") == 0) {
            if (++i >= argc || !parse_ll(argv[i], &cfg->step_hz)) {
                fprintf(stderr, "ERROR: --step requires an integer Hz value\n");
                return false;
            }
        } else if (strcmp(arg, "--rate") == 0) {
            if (++i >= argc || !parse_ll(argv[i], &cfg->sample_rate_hz)) {
                fprintf(stderr, "ERROR: --rate requires an integer Hz value\n");
                return false;
            }
        } else if (strcmp(arg, "--bw") == 0) {
            if (++i >= argc || !parse_ll(argv[i], &cfg->bandwidth_hz)) {
                fprintf(stderr, "ERROR: --bw requires an integer Hz value\n");
                return false;
            }
        } else if (strcmp(arg, "--fft") == 0) {
            if (++i >= argc || !parse_size_value(argv[i], &cfg->fft_size)) {
                fprintf(stderr, "ERROR: --fft requires an integer size\n");
                return false;
            }
        } else if (strcmp(arg, "--avg") == 0) {
            if (++i >= argc || !parse_int_value(argv[i], &cfg->averages)) {
                fprintf(stderr, "ERROR: --avg requires an integer count\n");
                return false;
            }
        } else if (strcmp(arg, "--top") == 0) {
            if (++i >= argc || !parse_int_value(argv[i], &cfg->top_peaks)) {
                fprintf(stderr, "ERROR: --top requires an integer count\n");
                return false;
            }
        } else if (strcmp(arg, "--threshold-db") == 0) {
            if (++i >= argc || !parse_double_value(argv[i], &cfg->threshold_db)) {
                fprintf(stderr, "ERROR: --threshold-db requires a numeric dB value\n");
                return false;
            }
        } else if (strcmp(arg, "--exclude-bins") == 0) {
            if (++i >= argc || !parse_int_value(argv[i], &cfg->exclude_bins)) {
                fprintf(stderr, "ERROR: --exclude-bins requires an integer count\n");
                return false;
            }
        } else if (strcmp(arg, "--dc-exclude-hz") == 0) {
            if (++i >= argc || !parse_double_value(argv[i], &cfg->dc_exclude_hz)) {
                fprintf(stderr, "ERROR: --dc-exclude-hz requires a numeric Hz value\n");
                return false;
            }
        } else if (strcmp(arg, "--settle-ms") == 0) {
            if (++i >= argc || !parse_int_value(argv[i], &cfg->settle_ms)) {
                fprintf(stderr, "ERROR: --settle-ms requires an integer milliseconds value\n");
                return false;
            }
        } else if (strcmp(arg, "--gain-mode") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "ERROR: --gain-mode requires a value\n");
                return false;
            }
            cfg->gain_mode = argv[i];
        } else if (strcmp(arg, "--gain-db") == 0) {
            if (++i >= argc || !parse_ll(argv[i], &cfg->manual_gain_db)) {
                fprintf(stderr, "ERROR: --gain-db requires an integer dB value\n");
                return false;
            }
            cfg->use_manual_gain = true;
        } else if (strcmp(arg, "--rx-mode") == 0) {
            if (++i >= argc || !parse_rx_mode_value(argv[i], &cfg->rx_mode)) {
                fprintf(stderr, "ERROR: --rx-mode requires auto, single, or dual\n");
                return false;
            }
        } else if (strcmp(arg, "--rx-combine") == 0) {
            if (++i >= argc || !parse_rx_combine_value(argv[i], &cfg->rx_combine)) {
                fprintf(stderr, "ERROR: --rx-combine requires max, average, or separate\n");
                return false;
            }
        } else if (strcmp(arg, "--csv") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "ERROR: --csv requires a filename\n");
                return false;
            }
            cfg->csv_file = argv[i];
            cfg->write_csv = true;
        } else {
            fprintf(stderr, "ERROR: Unknown option: %s\n", arg);
            return false;
        }
    }

    if (cfg->start_hz <= 0 || cfg->stop_hz <= 0) {
        fprintf(stderr, "ERROR: start and stop frequencies must be greater than zero\n");
        return false;
    }

    if (cfg->stop_hz < cfg->start_hz) {
        fprintf(stderr, "ERROR: stop frequency must be greater than or equal to start frequency\n");
        return false;
    }

    if (cfg->step_hz <= 0) {
        fprintf(stderr, "ERROR: step frequency must be greater than zero\n");
        return false;
    }

    if (cfg->sample_rate_hz <= 0) {
        fprintf(stderr, "ERROR: sample rate must be greater than zero\n");
        return false;
    }

    if (cfg->bandwidth_hz <= 0) {
        cfg->bandwidth_hz = cfg->sample_rate_hz;
    }

    if (cfg->fft_size < 256 || !is_power_of_two(cfg->fft_size)) {
        fprintf(stderr, "ERROR: FFT size must be a power of two and at least 256\n");
        return false;
    }

    if (cfg->averages <= 0 || cfg->top_peaks <= 0) {
        fprintf(stderr, "ERROR: averages and top peaks must be greater than zero\n");
        return false;
    }

    if (cfg->exclude_bins < 0 || cfg->settle_ms < 0 || cfg->dc_exclude_hz < 0.0) {
        fprintf(stderr, "ERROR: exclude bins, settle ms, and DC exclude Hz must not be negative\n");
        return false;
    }

    cfg->buffer_samples = cfg->fft_size;

    if (strcmp(cfg->gain_mode, "manual") == 0) {
        cfg->use_manual_gain = true;
    }

    return true;
}

static int write_attr_ll(struct iio_channel *ch, const char *attr, long long value)
{
    if (!ch) {
        fprintf(stderr, "WARNING: missing channel for attr %s\n", attr);
        return -1;
    }

    int ret = iio_channel_attr_write_longlong(ch, attr, value);
    if (ret < 0) {
        fprintf(stderr, "WARNING: could not set %s = %lld, ret=%d\n", attr, value, ret);
    }

    return ret;
}

static int write_attr_str(struct iio_channel *ch, const char *attr, const char *value)
{
    if (!ch) {
        fprintf(stderr, "WARNING: missing channel for attr %s\n", attr);
        return -1;
    }

    int ret = iio_channel_attr_write(ch, attr, value);
    if (ret < 0) {
        fprintf(stderr, "WARNING: could not set %s = %s, ret=%d\n", attr, value, ret);
    }

    return ret;
}

static double bin_offset_hz(size_t bin, size_t fft_size, double sample_rate)
{
    if (bin < fft_size / 2) {
        return ((double)bin * sample_rate) / (double)fft_size;
    }

    return (((double)bin - (double)fft_size) * sample_rate) / (double)fft_size;
}

static int compare_double(const void *a, const void *b)
{
    double da = *(const double *)a;
    double db = *(const double *)b;

    if (da < db) {
        return -1;
    }

    if (da > db) {
        return 1;
    }

    return 0;
}

static bool is_dc_region(size_t bin, size_t fft_size, double sample_rate, double dc_exclude_hz)
{
    double offset = bin_offset_hz(bin, fft_size, sample_rate);
    return fabs(offset) <= dc_exclude_hz;
}

static void block_nearby_bins(unsigned char *blocked, size_t fft_size, size_t center, int exclude_bins)
{
    for (int d = -exclude_bins; d <= exclude_bins; d++) {
        long idx = (long)center + d;

        while (idx < 0) {
            idx += (long)fft_size;
        }

        while (idx >= (long)fft_size) {
            idx -= (long)fft_size;
        }

        blocked[idx] = 1;
    }
}

static bool load_fft_input_from_channels(struct iio_buffer *rxbuf,
                                         struct iio_channel *ch_i,
                                         struct iio_channel *ch_q,
                                         size_t fft_size,
                                         fftw_complex *fft_in)
{
    char *p_i = (char *)iio_buffer_first(rxbuf, ch_i);
    char *p_q = (char *)iio_buffer_first(rxbuf, ch_q);
    char *p_end = (char *)iio_buffer_end(rxbuf);
    ptrdiff_t p_inc = iio_buffer_step(rxbuf);

    if (!p_i || !p_q || p_inc <= 0) {
        return false;
    }

    size_t n = 0;

    for (; p_i < p_end && p_q < p_end && n < fft_size; p_i += p_inc, p_q += p_inc, n++) {
        int16_t i_sample = 0;
        int16_t q_sample = 0;

        memcpy(&i_sample, p_i, sizeof(int16_t));
        memcpy(&q_sample, p_q, sizeof(int16_t));

        double window = 0.5 * (1.0 - cos((2.0 * PI_CONST * (double)n) / (double)(fft_size - 1)));

        fft_in[n][0] = ((double)i_sample / 32768.0) * window;
        fft_in[n][1] = ((double)q_sample / 32768.0) * window;
    }

    while (n < fft_size) {
        fft_in[n][0] = 0.0;
        fft_in[n][1] = 0.0;
        n++;
    }

    return true;
}

static void cleanup(
    struct iio_buffer *rxbuf,
    struct iio_context *ctx,
    fftw_plan plan,
    fftw_complex *fft_in,
    fftw_complex *fft_out,
    double *avg_power,
    double *power_db,
    double *sort_copy,
    unsigned char *blocked_bins,
    FILE *csv)
{
    if (csv) {
        fclose(csv);
    }

    if (rxbuf) {
        iio_buffer_destroy(rxbuf);
    }

    if (ctx) {
        iio_context_destroy(ctx);
    }

    if (plan) {
        fftw_destroy_plan(plan);
    }

    if (fft_in) {
        fftw_free(fft_in);
    }

    if (fft_out) {
        fftw_free(fft_out);
    }

    free(avg_power);
    free(power_db);
    free(sort_copy);
    free(blocked_bins);
}

int main(int argc, char **argv)
{
    app_config_t cfg;

    if (!parse_args(argc, argv, &cfg)) {
        print_usage(argv[0]);
        return 1;
    }

    printf("\n");
    printf("Pluto+ Sweep Scanner\n");
    printf("--------------------\n");
    printf("URI:              %s\n", cfg.uri);
    printf("Start frequency:  %lld Hz\n", cfg.start_hz);
    printf("Stop frequency:   %lld Hz\n", cfg.stop_hz);
    printf("Step frequency:   %lld Hz\n", cfg.step_hz);
    printf("Sample rate:      %lld Hz\n", cfg.sample_rate_hz);
    printf("RF bandwidth:     %lld Hz\n", cfg.bandwidth_hz);
    printf("FFT size:         %zu\n", cfg.fft_size);
    printf("Averages:         %d\n", cfg.averages);
    printf("Threshold:        %.2f dB over noise\n", cfg.threshold_db);
    printf("Top per step:     %d\n", cfg.top_peaks);
    printf("DC exclude:       %.1f Hz\n", cfg.dc_exclude_hz);
    printf("Settle:           %d ms\n", cfg.settle_ms);
    printf("Gain mode:        %s\n", cfg.gain_mode);
    printf("Requested RX mode:%s\n", cfg.rx_mode == RX_MODE_AUTO ? " auto" : (cfg.rx_mode == RX_MODE_SINGLE ? " single" : " dual"));
    printf("RX combine:       %s\n", rx_combine_name(cfg.rx_combine));

    if (cfg.use_manual_gain) {
        printf("Manual gain:      %lld dB\n", cfg.manual_gain_db);
    }

    if (cfg.write_csv) {
        printf("CSV output:       %s\n", cfg.csv_file);
    }

    double bin_width_hz = (double)cfg.sample_rate_hz / (double)cfg.fft_size;
    printf("FFT bin width:    %.2f Hz\n", bin_width_hz);
    printf("\n");

    struct iio_context *ctx = NULL;
    struct iio_buffer *rxbuf = NULL;
    FILE *csv = NULL;

    fftw_complex *fft_in = NULL;
    fftw_complex *fft_out = NULL;
    fftw_plan plan = NULL;

    double *avg_power = NULL;
    double *power_db = NULL;
    double *sort_copy = NULL;
    double *tmp_power = NULL;
    unsigned char *blocked_bins = NULL;

    ctx = iio_create_context_from_uri(cfg.uri);
    if (!ctx) {
        fprintf(stderr, "ERROR: Could not open IIO context: %s\n", cfg.uri);
        fprintf(stderr, "Try: iio_info -u %s\n", cfg.uri);
        cleanup(rxbuf, ctx, plan, fft_in, fft_out, avg_power, power_db, sort_copy, blocked_bins, csv);
        return 1;
    }

    struct iio_device *phy = iio_context_find_device(ctx, "ad9361-phy");
    struct iio_device *rxdev = iio_context_find_device(ctx, "cf-ad9361-lpc");

    if (!phy) {
        fprintf(stderr, "ERROR: Could not find ad9361-phy\n");
        cleanup(rxbuf, ctx, plan, fft_in, fft_out, avg_power, power_db, sort_copy, blocked_bins, csv);
        return 1;
    }

    if (!rxdev) {
        fprintf(stderr, "ERROR: Could not find cf-ad9361-lpc RX buffer device\n");
        cleanup(rxbuf, ctx, plan, fft_in, fft_out, avg_power, power_db, sort_copy, blocked_bins, csv);
        return 1;
    }

    struct iio_channel *rx0_i = iio_device_find_channel(rxdev, "voltage0", false);
    struct iio_channel *rx0_q = iio_device_find_channel(rxdev, "voltage1", false);
    struct iio_channel *rx2_i = iio_device_find_channel(rxdev, "voltage2", false);
    struct iio_channel *rx2_q = iio_device_find_channel(rxdev, "voltage3", false);

    struct iio_channel *phy_rx0 = iio_device_find_channel(phy, "voltage0", false);
    struct iio_channel *phy_rx1 = iio_device_find_channel(phy, "voltage1", false);
    struct iio_channel *rx_lo = iio_device_find_channel(phy, "altvoltage0", true);

    if (!rx0_i || !rx0_q || !phy_rx0 || !rx_lo) {
        fprintf(stderr, "ERROR: Missing required Pluto-compatible RX1 channels\n");
        cleanup(rxbuf, ctx, plan, fft_in, fft_out, avg_power, power_db, sort_copy, blocked_bins, csv);
        return 1;
    }

    bool rx2_ok = (rx2_i && rx2_q);
    bool use_dual = false;

    if (cfg.rx_mode == RX_MODE_DUAL) {
        if (!rx2_ok) {
            fprintf(stderr, "ERROR: --rx-mode dual requested, but RX2 voltage2/voltage3 channels are missing\n");
            cleanup(rxbuf, ctx, plan, fft_in, fft_out, avg_power, power_db, sort_copy, blocked_bins, csv);
            return 1;
        }
        use_dual = true;
    } else if (cfg.rx_mode == RX_MODE_AUTO) {
        use_dual = rx2_ok;
    }

    printf("RX1 channels: voltage0/voltage1 OK\n");
    printf("RX2 channels: voltage2/voltage3 %s\n", rx2_ok ? "OK" : "MISSING");
    printf("Effective RX mode: %s\n", use_dual ? "dual" : "single");
    printf("Dual available: %s\n", rx2_ok ? "yes" : "no");
    if (cfg.rx_combine == RX_COMBINE_SEPARATE && use_dual) {
        printf("Note: --rx-combine separate is accepted; sweep CSV remains compatible and uses max peak selection.\n");
    }

    printf("Configuring Pluto+ baseband...\n");

    int rate_ret = ad9361_set_bb_rate(phy, (unsigned long)cfg.sample_rate_hz);
    if (rate_ret < 0) {
        fprintf(stderr, "ERROR: ad9361_set_bb_rate(%lld) failed, ret=%d\n",
                cfg.sample_rate_hz,
                rate_ret);
        cleanup(rxbuf, ctx, plan, fft_in, fft_out, avg_power, power_db, sort_copy, blocked_bins, csv);
        return 1;
    }

    write_attr_ll(phy_rx0, "rf_bandwidth", cfg.bandwidth_hz);
    write_attr_str(phy_rx0, "gain_control_mode", cfg.gain_mode);

    if (use_dual && phy_rx1) {
        write_attr_ll(phy_rx1, "sampling_frequency", cfg.sample_rate_hz);
        write_attr_ll(phy_rx1, "rf_bandwidth", cfg.bandwidth_hz);
        write_attr_str(phy_rx1, "gain_control_mode", cfg.gain_mode);
    }

    if (cfg.use_manual_gain) {
        write_attr_ll(phy_rx0, "hardwaregain", cfg.manual_gain_db);
        if (use_dual && phy_rx1) {
            write_attr_ll(phy_rx1, "hardwaregain", cfg.manual_gain_db);
        }
    }

    iio_channel_enable(rx0_i);
    iio_channel_enable(rx0_q);
    if (use_dual) {
        iio_channel_enable(rx2_i);
        iio_channel_enable(rx2_q);
    }

    rxbuf = iio_device_create_buffer(rxdev, cfg.buffer_samples, false);
    if (!rxbuf) {
        fprintf(stderr, "ERROR: Could not create RX buffer\n");
        cleanup(rxbuf, ctx, plan, fft_in, fft_out, avg_power, power_db, sort_copy, blocked_bins, csv);
        return 1;
    }

    fft_in = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * cfg.fft_size);
    fft_out = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * cfg.fft_size);
    avg_power = (double *)calloc(cfg.fft_size, sizeof(double));
    power_db = (double *)calloc(cfg.fft_size, sizeof(double));
    sort_copy = (double *)calloc(cfg.fft_size, sizeof(double));
    tmp_power = (double *)calloc(cfg.fft_size, sizeof(double));
    blocked_bins = (unsigned char *)calloc(cfg.fft_size, sizeof(unsigned char));

    if (!fft_in || !fft_out || !avg_power || !power_db || !sort_copy || !tmp_power || !blocked_bins) {
        fprintf(stderr, "ERROR: Could not allocate FFT buffers\n");
        cleanup(rxbuf, ctx, plan, fft_in, fft_out, avg_power, power_db, sort_copy, blocked_bins, csv);
        return 1;
    }

    plan = fftw_plan_dft_1d((int)cfg.fft_size, fft_in, fft_out, FFTW_FORWARD, FFTW_ESTIMATE);
    if (!plan) {
        fprintf(stderr, "ERROR: Could not create FFTW plan\n");
        cleanup(rxbuf, ctx, plan, fft_in, fft_out, avg_power, power_db, sort_copy, blocked_bins, csv);
        return 1;
    }

    if (cfg.write_csv) {
        csv = fopen(cfg.csv_file, "w");
        if (!csv) {
            fprintf(stderr, "ERROR: Could not open CSV file: %s\n", cfg.csv_file);
            cleanup(rxbuf, ctx, plan, fft_in, fft_out, avg_power, power_db, sort_copy, blocked_bins, csv);
            return 1;
        }

        fprintf(csv, "center_hz,peak_hz,offset_hz,power_db,noise_floor_db,snr_db,rank\n");
    }

    printf("\nStarting sweep...\n\n");

    for (long long center_hz = cfg.start_hz;
         center_hz <= cfg.stop_hz;
         center_hz += cfg.step_hz) {

        printf("Tuning center: %.6f MHz\n", (double)center_hz / 1e6);

        write_attr_ll(rx_lo, "frequency", center_hz);
        sleep_ms(cfg.settle_ms);

        /*
           Discard one buffer after retune to reduce stale samples.
        */
        iio_buffer_refill(rxbuf);

        memset(avg_power, 0, sizeof(double) * cfg.fft_size);

        for (int avg = 0; avg < cfg.averages; avg++) {
            ssize_t nbytes = iio_buffer_refill(rxbuf);

            if (nbytes < 0) {
                fprintf(stderr, "ERROR: RX buffer refill failed: %zd\n", nbytes);
                cleanup(rxbuf, ctx, plan, fft_in, fft_out, avg_power, power_db, sort_copy, blocked_bins, csv);
                return 1;
            }

            if (!load_fft_input_from_channels(rxbuf, rx0_i, rx0_q, cfg.fft_size, fft_in)) {
                fprintf(stderr, "ERROR: Invalid RX1 buffer layout\n");
                cleanup(rxbuf, ctx, plan, fft_in, fft_out, avg_power, power_db, sort_copy, blocked_bins, csv);
                free(tmp_power);
                return 1;
            }

            fftw_execute(plan);

            for (size_t k = 0; k < cfg.fft_size; k++) {
                double re = fft_out[k][0];
                double im = fft_out[k][1];
                tmp_power[k] = re * re + im * im;
            }

            if (use_dual) {
                if (!load_fft_input_from_channels(rxbuf, rx2_i, rx2_q, cfg.fft_size, fft_in)) {
                    fprintf(stderr, "ERROR: Invalid RX2 buffer layout\n");
                    cleanup(rxbuf, ctx, plan, fft_in, fft_out, avg_power, power_db, sort_copy, blocked_bins, csv);
                    free(tmp_power);
                    return 1;
                }

                fftw_execute(plan);

                for (size_t k = 0; k < cfg.fft_size; k++) {
                    double re = fft_out[k][0];
                    double im = fft_out[k][1];
                    double mag2_rx2 = re * re + im * im;
                    double mag2 = tmp_power[k];

                    if (cfg.rx_combine == RX_COMBINE_AVERAGE) {
                        mag2 = 0.5 * (tmp_power[k] + mag2_rx2);
                    } else {
                        if (mag2_rx2 > mag2) {
                            mag2 = mag2_rx2;
                        }
                    }

                    avg_power[k] += mag2;
                }
            } else {
                for (size_t k = 0; k < cfg.fft_size; k++) {
                    avg_power[k] += tmp_power[k];
                }
            }
        }

        size_t valid_count = 0;

        for (size_t k = 0; k < cfg.fft_size; k++) {
            avg_power[k] /= (double)cfg.averages;
            power_db[k] = 10.0 * log10(avg_power[k] + 1e-30);

            if (!is_dc_region(k, cfg.fft_size, (double)cfg.sample_rate_hz, cfg.dc_exclude_hz)) {
                sort_copy[valid_count++] = power_db[k];
            }
        }

        if (valid_count == 0) {
            printf("  No valid FFT bins after DC exclusion.\n\n");
            continue;
        }

        qsort(sort_copy, valid_count, sizeof(double), compare_double);

        double noise_floor_db = sort_copy[valid_count / 2];

        memset(blocked_bins, 0, cfg.fft_size);

        printf("  Noise floor: %.2f dB\n", noise_floor_db);
        printf("  Detected peaks:\n");
        printf("  Rank  Peak MHz       Offset kHz      Power dB     SNR dB\n");
        printf("  ----  --------       ----------      --------     ------\n");

        int found = 0;

        for (int rank = 1; rank <= cfg.top_peaks; rank++) {
            size_t best_bin = 0;
            double best_power_db = -1e300;

            for (size_t k = 0; k < cfg.fft_size; k++) {
                if (blocked_bins[k]) {
                    continue;
                }

                if (is_dc_region(k, cfg.fft_size, (double)cfg.sample_rate_hz, cfg.dc_exclude_hz)) {
                    continue;
                }

                if (power_db[k] > best_power_db) {
                    best_power_db = power_db[k];
                    best_bin = k;
                }
            }

            double snr_db = best_power_db - noise_floor_db;

            if (snr_db < cfg.threshold_db) {
                break;
            }

            double offset_hz = bin_offset_hz(best_bin, cfg.fft_size, (double)cfg.sample_rate_hz);
            double peak_hz = (double)center_hz + offset_hz;

            printf("  %4d  %8.6f  %14.3f  %12.2f  %9.2f\n",
                   rank,
                   peak_hz / 1e6,
                   offset_hz / 1e3,
                   best_power_db,
                   snr_db);

            if (csv) {
                fprintf(csv, "%lld,%.3f,%.3f,%.2f,%.2f,%.2f,%d\n",
                        center_hz,
                        peak_hz,
                        offset_hz,
                        best_power_db,
                        noise_floor_db,
                        snr_db,
                        rank);
            }

            found++;
            block_nearby_bins(blocked_bins, cfg.fft_size, best_bin, cfg.exclude_bins);
        }

        if (found == 0) {
            printf("  No peaks above threshold.\n");
        }

        printf("\n");
        fflush(stdout);

        if (center_hz > cfg.stop_hz - cfg.step_hz) {
            break;
        }
    }

    printf("Sweep complete.\n");

    if (cfg.write_csv) {
        printf("CSV written: %s\n", cfg.csv_file);
    }

    free(tmp_power);
    cleanup(rxbuf, ctx, plan, fft_in, fft_out, avg_power, power_db, sort_copy, blocked_bins, csv);
    return 0;
}
