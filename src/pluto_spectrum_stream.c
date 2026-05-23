#include <iio.h>
#include <ad9361.h>
#include <fftw3.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*
   pluto_spectrum_stream.c

   Native Pluto+/Pluto-compatible live FFT spectrum streamer.

   Outputs lines to stdout:

     STATUS,<message>
     SPECTRUM,<frame>,<center_hz>,<sample_rate_hz>,<fft_size>,<bin_width_hz>,<min_offset_hz>,<db0>,<db1>,...

   The dB bins are FFT-shifted:
     db0 is the negative edge of the span
     middle bin is near DC
     last bin is the positive edge of the span

   Designed for a WPF GUI wrapper to launch and parse stdout.
*/

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
    int interval_ms;
    int frames;

    const char *gain_mode;
    bool use_manual_gain;
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
    printf("Pluto+ Live Spectrum Streamer\n");
    printf("\n");
    printf("Usage:\n");
    printf("  %s [options]\n", prog);
    printf("\n");
    printf("Options:\n");
    printf("  --uri <uri>             IIO URI, default ip:192.168.2.1\n");
    printf("  --freq <hz>             RX center frequency Hz, default 146520000\n");
    printf("  --rate <hz>             Sample rate Hz, default 1000000\n");
    printf("  --bw <hz>               RF bandwidth Hz, default equals sample rate\n");
    printf("  --fft <size>            FFT size, default 1024\n");
    printf("  --avg <count>           FFT averages per display frame, default 2\n");
    printf("  --interval-ms <ms>      Delay after each display frame, default 200\n");
    printf("  --frames <count>        Number of frames to output, 0 = forever, default 0\n");
    printf("  --gain-mode <mode>      slow_attack, fast_attack, manual, default slow_attack\n");
    printf("  --gain-db <db>          Manual RX gain dB, implies manual gain mode\n");
    printf("  --help                  Show this help\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s --freq 146520000 --rate 1000000 --fft 1024\n", prog);
    printf("  %s --freq 162550000 --rate 1000000 --avg 4\n", prog);
    printf("  %s --freq 125000000 --rate 2000000 --bw 2000000 --fft 2048\n", prog);
    printf("  %s --freq 146520000 --gain-mode manual --gain-db 40\n", prog);
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

    cfg->freq_hz = 146520000LL;
    cfg->sample_rate_hz = 1000000LL;
    cfg->bandwidth_hz = 0;
    cfg->manual_gain_db = 30;

    cfg->fft_size = 1024;
    cfg->buffer_samples = 1024;

    cfg->averages = 2;
    cfg->interval_ms = 200;
    cfg->frames = 0;

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
        } else if (strcmp(arg, "--interval-ms") == 0) {
            if (++i >= argc || !parse_int_value(argv[i], &cfg->interval_ms)) {
                fprintf(stderr, "ERROR: --interval-ms requires an integer value\n");
                return false;
            }
        } else if (strcmp(arg, "--frames") == 0) {
            if (++i >= argc || !parse_int_value(argv[i], &cfg->frames)) {
                fprintf(stderr, "ERROR: --frames requires an integer count\n");
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
            cfg->gain_mode = "manual";
            cfg->use_manual_gain = true;
        } else {
            fprintf(stderr, "ERROR: Unknown option: %s\n", arg);
            return false;
        }
    }

    if (cfg->freq_hz <= 0 || cfg->sample_rate_hz <= 0) {
        fprintf(stderr, "ERROR: frequency and sample rate must be greater than zero\n");
        return false;
    }

    if (cfg->bandwidth_hz <= 0) {
        cfg->bandwidth_hz = cfg->sample_rate_hz;
    }

    if (cfg->fft_size < 256 || !is_power_of_two(cfg->fft_size)) {
        fprintf(stderr, "ERROR: FFT size must be a power of two and at least 256\n");
        return false;
    }

    if (cfg->averages <= 0 || cfg->interval_ms < 0 || cfg->frames < 0) {
        fprintf(stderr, "ERROR: averages must be > 0, interval/frames must not be negative\n");
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

static void cleanup(
    struct iio_buffer *rxbuf,
    struct iio_context *ctx,
    fftw_plan plan,
    fftw_complex *fft_in,
    fftw_complex *fft_out,
    double *avg_power,
    double *power_db)
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

    free(avg_power);
    free(power_db);
}

int main(int argc, char **argv)
{
    app_config_t cfg;

    if (!parse_args(argc, argv, &cfg)) {
        print_usage(argv[0]);
        return 1;
    }

    printf("STATUS,Opening IIO context %s\n", cfg.uri);
    fflush(stdout);

    struct iio_context *ctx = NULL;
    struct iio_buffer *rxbuf = NULL;
    fftw_complex *fft_in = NULL;
    fftw_complex *fft_out = NULL;
    double *avg_power = NULL;
    double *power_db = NULL;
    fftw_plan plan = NULL;

    ctx = iio_create_context_from_uri(cfg.uri);
    if (!ctx) {
        fprintf(stderr, "ERROR: Could not open IIO context: %s\n", cfg.uri);
        return 1;
    }

    struct iio_device *phy = iio_context_find_device(ctx, "ad9361-phy");
    struct iio_device *rxdev = iio_context_find_device(ctx, "cf-ad9361-lpc");

    if (!phy || !rxdev) {
        fprintf(stderr, "ERROR: Missing ad9361-phy or cf-ad9361-lpc\n");
        cleanup(rxbuf, ctx, plan, fft_in, fft_out, avg_power, power_db);
        return 1;
    }

    struct iio_channel *rx0_i = iio_device_find_channel(rxdev, "voltage0", false);
    struct iio_channel *rx0_q = iio_device_find_channel(rxdev, "voltage1", false);
    struct iio_channel *phy_rx0 = iio_device_find_channel(phy, "voltage0", false);
    struct iio_channel *rx_lo = iio_device_find_channel(phy, "altvoltage0", true);

    if (!rx0_i || !rx0_q || !phy_rx0 || !rx_lo) {
        fprintf(stderr, "ERROR: Missing required Pluto-compatible RX channels\n");
        cleanup(rxbuf, ctx, plan, fft_in, fft_out, avg_power, power_db);
        return 1;
    }

    printf("STATUS,Configuring center=%lld rate=%lld bw=%lld fft=%zu avg=%d gain=%s\n",
           cfg.freq_hz,
           cfg.sample_rate_hz,
           cfg.bandwidth_hz,
           cfg.fft_size,
           cfg.averages,
           cfg.gain_mode);
    fflush(stdout);

    int rate_ret = ad9361_set_bb_rate(phy, (unsigned long)cfg.sample_rate_hz);
    if (rate_ret < 0) {
        fprintf(stderr, "ERROR: ad9361_set_bb_rate(%lld) failed, ret=%d\n", cfg.sample_rate_hz, rate_ret);
        cleanup(rxbuf, ctx, plan, fft_in, fft_out, avg_power, power_db);
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
        cleanup(rxbuf, ctx, plan, fft_in, fft_out, avg_power, power_db);
        return 1;
    }

    fft_in = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * cfg.fft_size);
    fft_out = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * cfg.fft_size);
    avg_power = (double *)calloc(cfg.fft_size, sizeof(double));
    power_db = (double *)calloc(cfg.fft_size, sizeof(double));

    if (!fft_in || !fft_out || !avg_power || !power_db) {
        fprintf(stderr, "ERROR: Could not allocate FFT buffers\n");
        cleanup(rxbuf, ctx, plan, fft_in, fft_out, avg_power, power_db);
        return 1;
    }

    plan = fftw_plan_dft_1d((int)cfg.fft_size, fft_in, fft_out, FFTW_FORWARD, FFTW_ESTIMATE);
    if (!plan) {
        fprintf(stderr, "ERROR: Could not create FFTW plan\n");
        cleanup(rxbuf, ctx, plan, fft_in, fft_out, avg_power, power_db);
        return 1;
    }

    /*
       Discard a few initial buffers after configuration.
    */
    for (int i = 0; i < 3; i++) {
        iio_buffer_refill(rxbuf);
    }

    const double bin_width_hz = (double)cfg.sample_rate_hz / (double)cfg.fft_size;
    const double min_offset_hz = -(double)cfg.sample_rate_hz / 2.0;

    printf("STATUS,Streaming started\n");
    fflush(stdout);

    int frame = 0;

    while (cfg.frames == 0 || frame < cfg.frames) {
        memset(avg_power, 0, sizeof(double) * cfg.fft_size);

        for (int avg = 0; avg < cfg.averages; avg++) {
            ssize_t nbytes = iio_buffer_refill(rxbuf);

            if (nbytes < 0) {
                fprintf(stderr, "ERROR: RX buffer refill failed: %zd\n", nbytes);
                cleanup(rxbuf, ctx, plan, fft_in, fft_out, avg_power, power_db);
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
                avg_power[k] += re * re + im * im;
            }
        }

        for (size_t k = 0; k < cfg.fft_size; k++) {
            avg_power[k] /= (double)cfg.averages;
            power_db[k] = 10.0 * log10(avg_power[k] + 1e-30);
        }

        printf("SPECTRUM,%d,%lld,%lld,%zu,%.6f,%.6f",
               frame,
               cfg.freq_hz,
               cfg.sample_rate_hz,
               cfg.fft_size,
               bin_width_hz,
               min_offset_hz);

        /*
           FFT-shift for display: negative frequencies first, positive second.
        */
        for (size_t i = 0; i < cfg.fft_size; i++) {
            size_t src = (i + cfg.fft_size / 2) % cfg.fft_size;
            printf(",%.2f", power_db[src]);
        }

        printf("\n");
        fflush(stdout);

        frame++;

        if (cfg.interval_ms > 0) {
            sleep_ms(cfg.interval_ms);
        }
    }

    printf("STATUS,Streaming complete\n");
    fflush(stdout);

    cleanup(rxbuf, ctx, plan, fft_in, fft_out, avg_power, power_db);
    return 0;
}
