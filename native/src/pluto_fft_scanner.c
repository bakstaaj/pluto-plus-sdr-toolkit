#include <iio.h>
#include <ad9361.h>
#include <fftw3.h>

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define PI_CONST 3.14159265358979323846

typedef struct {
    const char *uri;

    long long freq_hz;
    long long sample_rate_hz;
    long long bandwidth_hz;
    long long manual_gain_db;

    size_t fft_size;
    size_t buffer_samples;

    int averages;
    int top_peaks;
    int exclude_bins;

    const char *gain_mode;
    bool use_manual_gain;
} app_config_t;

static void print_usage(const char *prog)
{
    printf("\n");
    printf("Pluto+ FFT Scanner - single RX Pluto-compatible mode\n");
    printf("\n");
    printf("Usage:\n");
    printf("  %s [options]\n", prog);
    printf("\n");
    printf("Options:\n");
    printf("  --uri <uri>             IIO URI, default ip:192.168.2.1\n");
    printf("  --freq <hz>             RX center frequency in Hz, default 100000000\n");
    printf("  --rate <hz>             Sample rate in Hz, default 1000000\n");
    printf("  --bw <hz>               RF bandwidth in Hz, default equals sample rate\n");
    printf("  --fft <size>            FFT size, default 16384\n");
    printf("  --avg <count>           Number of FFT averages, default 8\n");
    printf("  --top <count>           Number of peaks to print, default 10\n");
    printf("  --exclude-bins <count>  Bins to suppress around each detected peak, default 8\n");
    printf("  --gain-mode <mode>      slow_attack, fast_attack, manual, default slow_attack\n");
    printf("  --gain-db <db>          Manual RX gain in dB, only used with manual gain mode\n");
    printf("  --help                  Show this help\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s --freq 100000000 --rate 1000000 --fft 16384 --avg 8\n", prog);
    printf("  %s --freq 146520000 --rate 1000000 --top 15\n", prog);
    printf("  %s --freq 125000000 --rate 2000000 --bw 2000000 --avg 16\n", prog);
    printf("  %s --freq 915000000 --rate 2000000 --gain-mode manual --gain-db 40\n", prog);
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

static bool is_power_of_two(size_t n)
{
    return n > 0 && ((n & (n - 1)) == 0);
}

static bool parse_args(int argc, char **argv, app_config_t *cfg)
{
    cfg->uri = "ip:192.168.2.1";

    cfg->freq_hz = 100000000LL;
    cfg->sample_rate_hz = 1000000LL;
    cfg->bandwidth_hz = 0;
    cfg->manual_gain_db = 30;

    cfg->fft_size = 16384;
    cfg->buffer_samples = 16384;

    cfg->averages = 8;
    cfg->top_peaks = 10;
    cfg->exclude_bins = 8;

    cfg->gain_mode = "slow_attack";
    cfg->use_manual_gain = false;

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
        } else if (strcmp(arg, "--freq") == 0) {
            if (++i >= argc || !parse_ll(argv[i], &cfg->freq_hz)) {
                fprintf(stderr, "ERROR: --freq requires an integer Hz value\n");
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
        } else if (strcmp(arg, "--exclude-bins") == 0) {
            if (++i >= argc || !parse_int_value(argv[i], &cfg->exclude_bins)) {
                fprintf(stderr, "ERROR: --exclude-bins requires an integer count\n");
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
        } else {
            fprintf(stderr, "ERROR: Unknown option: %s\n", arg);
            return false;
        }
    }

    if (cfg->sample_rate_hz <= 0) {
        fprintf(stderr, "ERROR: sample rate must be greater than zero\n");
        return false;
    }

    if (cfg->freq_hz <= 0) {
        fprintf(stderr, "ERROR: frequency must be greater than zero\n");
        return false;
    }

    if (cfg->fft_size < 256) {
        fprintf(stderr, "ERROR: FFT size must be at least 256\n");
        return false;
    }

    if (!is_power_of_two(cfg->fft_size)) {
        fprintf(stderr, "ERROR: FFT size should be a power of two, for example 4096, 8192, 16384, 32768\n");
        return false;
    }

    if (cfg->averages <= 0) {
        fprintf(stderr, "ERROR: average count must be greater than zero\n");
        return false;
    }

    if (cfg->top_peaks <= 0) {
        fprintf(stderr, "ERROR: top peak count must be greater than zero\n");
        return false;
    }

    if (cfg->exclude_bins < 0) {
        fprintf(stderr, "ERROR: exclude bins must not be negative\n");
        return false;
    }

    if (cfg->bandwidth_hz <= 0) {
        cfg->bandwidth_hz = cfg->sample_rate_hz;
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

static void cleanup(
    struct iio_buffer *rxbuf,
    struct iio_context *ctx,
    fftw_plan plan,
    fftw_complex *fft_in,
    fftw_complex *fft_out,
    double *avg_power,
    unsigned char *blocked_bins)
{
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

    if (avg_power) {
        free(avg_power);
    }

    if (blocked_bins) {
        free(blocked_bins);
    }
}

static double bin_offset_hz(size_t bin, size_t fft_size, double sample_rate)
{
    if (bin < fft_size / 2) {
        return ((double)bin * sample_rate) / (double)fft_size;
    }

    return (((double)bin - (double)fft_size) * sample_rate) / (double)fft_size;
}

static void block_nearby_bins(unsigned char *blocked, size_t fft_size, size_t center, int exclude_bins)
{
    long start = (long)center - exclude_bins;
    long end = (long)center + exclude_bins;

    if (start < 0) {
        start = 0;
    }

    if (end >= (long)fft_size) {
        end = (long)fft_size - 1;
    }

    for (long i = start; i <= end; i++) {
        blocked[i] = 1;
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
    printf("Pluto+ FFT Scanner\n");
    printf("------------------\n");
    printf("URI:              %s\n", cfg.uri);
    printf("Center frequency: %lld Hz\n", cfg.freq_hz);
    printf("Sample rate:      %lld Hz\n", cfg.sample_rate_hz);
    printf("RF bandwidth:     %lld Hz\n", cfg.bandwidth_hz);
    printf("FFT size:         %zu\n", cfg.fft_size);
    printf("Averages:         %d\n", cfg.averages);
    printf("Top peaks:        %d\n", cfg.top_peaks);
    printf("Gain mode:        %s\n", cfg.gain_mode);

    if (cfg.use_manual_gain) {
        printf("Manual gain:      %lld dB\n", cfg.manual_gain_db);
    }

    double bin_width_hz = (double)cfg.sample_rate_hz / (double)cfg.fft_size;
    printf("FFT bin width:    %.2f Hz\n", bin_width_hz);
    printf("\n");

    struct iio_context *ctx = NULL;
    struct iio_buffer *rxbuf = NULL;

    fftw_complex *fft_in = NULL;
    fftw_complex *fft_out = NULL;
    double *avg_power = NULL;
    unsigned char *blocked_bins = NULL;
    fftw_plan plan = NULL;

    ctx = iio_create_context_from_uri(cfg.uri);
    if (!ctx) {
        fprintf(stderr, "ERROR: Could not open IIO context: %s\n", cfg.uri);
        fprintf(stderr, "Try this first:\n");
        fprintf(stderr, "  iio_info -u %s\n", cfg.uri);
        cleanup(rxbuf, ctx, plan, fft_in, fft_out, avg_power, blocked_bins);
        return 1;
    }

    struct iio_device *phy = iio_context_find_device(ctx, "ad9361-phy");
    struct iio_device *rxdev = iio_context_find_device(ctx, "cf-ad9361-lpc");

    if (!phy) {
        fprintf(stderr, "ERROR: Could not find ad9361-phy\n");
        cleanup(rxbuf, ctx, plan, fft_in, fft_out, avg_power, blocked_bins);
        return 1;
    }

    if (!rxdev) {
        fprintf(stderr, "ERROR: Could not find cf-ad9361-lpc RX buffer device\n");
        cleanup(rxbuf, ctx, plan, fft_in, fft_out, avg_power, blocked_bins);
        return 1;
    }

    struct iio_channel *rx0_i = iio_device_find_channel(rxdev, "voltage0", false);
    struct iio_channel *rx0_q = iio_device_find_channel(rxdev, "voltage1", false);

    if (!rx0_i || !rx0_q) {
        fprintf(stderr, "ERROR: Could not find RX0 I/Q scan channels voltage0 and voltage1\n");
        cleanup(rxbuf, ctx, plan, fft_in, fft_out, avg_power, blocked_bins);
        return 1;
    }

    struct iio_channel *phy_rx0 = iio_device_find_channel(phy, "voltage0", false);
    struct iio_channel *rx_lo = iio_device_find_channel(phy, "altvoltage0", true);

    if (!phy_rx0) {
        fprintf(stderr, "ERROR: Could not find PHY RX0 control channel voltage0\n");
        cleanup(rxbuf, ctx, plan, fft_in, fft_out, avg_power, blocked_bins);
        return 1;
    }

    if (!rx_lo) {
        fprintf(stderr, "ERROR: Could not find RX LO channel altvoltage0\n");
        cleanup(rxbuf, ctx, plan, fft_in, fft_out, avg_power, blocked_bins);
        return 1;
    }

    printf("Configuring Pluto+...\n");

    int rate_ret = ad9361_set_bb_rate(phy, (unsigned long)cfg.sample_rate_hz);
    if (rate_ret < 0) {
        fprintf(stderr, "ERROR: ad9361_set_bb_rate(%lld) failed, ret=%d\n",
                cfg.sample_rate_hz,
                rate_ret);
        cleanup(rxbuf, ctx, plan, fft_in, fft_out, avg_power, blocked_bins);
        return 1;
    }

    write_attr_ll(phy_rx0, "rf_bandwidth", cfg.bandwidth_hz);
    write_attr_str(phy_rx0, "gain_control_mode", cfg.gain_mode);

    if (cfg.use_manual_gain) {
        write_attr_ll(phy_rx0, "hardwaregain", cfg.manual_gain_db);
    }

    write_attr_ll(rx_lo, "frequency", cfg.freq_hz);

    iio_channel_enable(rx0_i);
    iio_channel_enable(rx0_q);

    rxbuf = iio_device_create_buffer(rxdev, cfg.buffer_samples, false);
    if (!rxbuf) {
        fprintf(stderr, "ERROR: Could not create RX buffer\n");
        cleanup(rxbuf, ctx, plan, fft_in, fft_out, avg_power, blocked_bins);
        return 1;
    }

    fft_in = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * cfg.fft_size);
    fft_out = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * cfg.fft_size);
    avg_power = (double *)calloc(cfg.fft_size, sizeof(double));
    blocked_bins = (unsigned char *)calloc(cfg.fft_size, sizeof(unsigned char));

    if (!fft_in || !fft_out || !avg_power || !blocked_bins) {
        fprintf(stderr, "ERROR: Could not allocate FFT buffers\n");
        cleanup(rxbuf, ctx, plan, fft_in, fft_out, avg_power, blocked_bins);
        return 1;
    }

    plan = fftw_plan_dft_1d((int)cfg.fft_size, fft_in, fft_out, FFTW_FORWARD, FFTW_ESTIMATE);
    if (!plan) {
        fprintf(stderr, "ERROR: Could not create FFTW plan\n");
        cleanup(rxbuf, ctx, plan, fft_in, fft_out, avg_power, blocked_bins);
        return 1;
    }

    printf("Capturing and averaging %d FFT frames...\n\n", cfg.averages);

    for (int avg = 0; avg < cfg.averages; avg++) {
        ssize_t nbytes = iio_buffer_refill(rxbuf);

        if (nbytes < 0) {
            fprintf(stderr, "ERROR: RX buffer refill failed: %zd\n", nbytes);
            cleanup(rxbuf, ctx, plan, fft_in, fft_out, avg_power, blocked_bins);
            return 1;
        }

        char *p_i = (char *)iio_buffer_first(rxbuf, rx0_i);
        char *p_q = (char *)iio_buffer_first(rxbuf, rx0_q);
        char *p_end = (char *)iio_buffer_end(rxbuf);
        ptrdiff_t p_inc = iio_buffer_step(rxbuf);

        size_t n = 0;

        for (; p_i < p_end && p_q < p_end && n < cfg.fft_size; p_i += p_inc, p_q += p_inc, n++) {
            int16_t i_sample = 0;
            int16_t q_sample = 0;

            memcpy(&i_sample, p_i, sizeof(int16_t));
            memcpy(&q_sample, p_q, sizeof(int16_t));

            double window = 0.5 * (1.0 - cos((2.0 * PI_CONST * (double)n) / (double)(cfg.fft_size - 1)));

            fft_in[n][0] = ((double)i_sample / 32768.0) * window;
            fft_in[n][1] = ((double)q_sample / 32768.0) * window;
        }

        while (n < cfg.fft_size) {
            fft_in[n][0] = 0.0;
            fft_in[n][1] = 0.0;
            n++;
        }

        fftw_execute(plan);

        for (size_t k = 0; k < cfg.fft_size; k++) {
            double re = fft_out[k][0];
            double im = fft_out[k][1];
            double mag2 = re * re + im * im;

            avg_power[k] += mag2;
        }

        printf("Averaged frame %d / %d\n", avg + 1, cfg.averages);
    }

    for (size_t k = 0; k < cfg.fft_size; k++) {
        avg_power[k] /= (double)cfg.averages;
    }

    printf("\nStrongest peaks:\n");
    printf("Rank  Frequency MHz      Offset kHz       Relative dB\n");
    printf("----  -------------      ----------       -----------\n");

    for (int rank = 1; rank <= cfg.top_peaks; rank++) {
        size_t best_bin = 0;
        double best_power = -1.0;

        for (size_t k = 0; k < cfg.fft_size; k++) {
            if (blocked_bins[k]) {
                continue;
            }

            if (avg_power[k] > best_power) {
                best_power = avg_power[k];
                best_bin = k;
            }
        }

        if (best_power <= 0.0) {
            break;
        }

        double offset_hz = bin_offset_hz(best_bin, cfg.fft_size, (double)cfg.sample_rate_hz);
        double abs_freq_hz = (double)cfg.freq_hz + offset_hz;
        double rel_db = 10.0 * log10(best_power + 1e-30);

        printf("%4d  %13.6f  %14.3f  %16.2f\n",
               rank,
               abs_freq_hz / 1e6,
               offset_hz / 1e3,
               rel_db);

        block_nearby_bins(blocked_bins, cfg.fft_size, best_bin, cfg.exclude_bins);
    }

    printf("\nDone.\n");

    cleanup(rxbuf, ctx, plan, fft_in, fft_out, avg_power, blocked_bins);
    return 0;
}
