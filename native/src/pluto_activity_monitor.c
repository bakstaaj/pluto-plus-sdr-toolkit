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
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define PI_CONST 3.14159265358979323846

typedef struct {
    double *freqs_hz;
    size_t count;
    size_t capacity;
} freq_list_t;

typedef struct {
    const char *uri;
    const char *input_file;
    const char *csv_file;

    long long sample_rate_hz;
    long long bandwidth_hz;
    long long manual_gain_db;

    size_t fft_size;
    size_t buffer_samples;

    int averages;
    int cycles;
    int settle_ms;
    int delay_ms;

    double threshold_db;
    double passband_hz;
    double if_offset_hz;
    double dc_exclude_hz;

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

static void now_string(char *buf, size_t size)
{
    time_t t = time(NULL);
    struct tm tmv;

#ifdef _WIN32
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif

    strftime(buf, size, "%Y-%m-%d %H:%M:%S", &tmv);
}

static void print_usage(const char *prog)
{
    printf("\n");
    printf("Pluto+ Activity Monitor - single RX Pluto-compatible mode\n");
    printf("\n");
    printf("Usage:\n");
    printf("  %s --in <grouped.csv> [options]\n", prog);
    printf("\n");
    printf("Input file:\n");
    printf("  The input may be a grouped CSV from pluto_scan_group.exe,\n");
    printf("  or a simple text file with one frequency in Hz per line.\n");
    printf("\n");
    printf("Options:\n");
    printf("  --uri <uri>             IIO URI, default ip:192.168.2.1\n");
    printf("  --in <file>             Frequency list or grouped CSV\n");
    printf("  --csv <file>            Activity log CSV, default activity_log.csv\n");
    printf("  --rate <hz>             Sample rate Hz, default 1000000\n");
    printf("  --bw <hz>               RF bandwidth Hz, default equals sample rate\n");
    printf("  --fft <size>            FFT size, default 16384\n");
    printf("  --avg <count>           FFT averages per measurement, default 4\n");
    printf("  --cycles <count>        Number of passes over the list, default 5\n");
    printf("  --threshold-db <db>     Active threshold over noise floor, default 10\n");
    printf("  --passband-hz <hz>      Detection window around IF, default 12500\n");
    printf("  --if-offset-hz <hz>     Tune target to this FFT offset, default 100000\n");
    printf("  --dc-exclude-hz <hz>    Ignore DC region around 0 Hz, default 10000\n");
    printf("  --settle-ms <ms>        Delay after tuning, default 100\n");
    printf("  --delay-ms <ms>         Delay between frequencies, default 0\n");
    printf("  --gain-mode <mode>      slow_attack, fast_attack, manual, default slow_attack\n");
    printf("  --gain-db <db>          Manual RX gain dB, implies manual gain mode\n");
    printf("  --help                  Show this help\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s --in 2m_grouped.csv --cycles 10 --csv 2m_activity.csv\n", prog);
    printf("  %s --in airband_grouped.csv --passband-hz 25000 --threshold-db 8\n", prog);
    printf("  %s --in 2m_grouped.csv --gain-mode manual --gain-db 40 --cycles 20\n", prog);
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

static bool is_power_of_two(size_t n)
{
    return n > 0 && ((n & (n - 1)) == 0);
}

static bool parse_args(int argc, char **argv, app_config_t *cfg)
{
    cfg->uri = "ip:192.168.2.1";
    cfg->input_file = NULL;
    cfg->csv_file = "activity_log.csv";

    cfg->sample_rate_hz = 1000000LL;
    cfg->bandwidth_hz = 0;
    cfg->manual_gain_db = 30;

    cfg->fft_size = 16384;
    cfg->buffer_samples = 16384;

    cfg->averages = 4;
    cfg->cycles = 5;
    cfg->settle_ms = 100;
    cfg->delay_ms = 0;

    cfg->threshold_db = 10.0;
    cfg->passband_hz = 12500.0;
    cfg->if_offset_hz = 100000.0;
    cfg->dc_exclude_hz = 10000.0;

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
        } else if (strcmp(arg, "--in") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "ERROR: --in requires a filename\n");
                return false;
            }
            cfg->input_file = argv[i];
        } else if (strcmp(arg, "--csv") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "ERROR: --csv requires a filename\n");
                return false;
            }
            cfg->csv_file = argv[i];
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
        } else if (strcmp(arg, "--cycles") == 0) {
            if (++i >= argc || !parse_int_value(argv[i], &cfg->cycles)) {
                fprintf(stderr, "ERROR: --cycles requires an integer count\n");
                return false;
            }
        } else if (strcmp(arg, "--threshold-db") == 0) {
            if (++i >= argc || !parse_double_value(argv[i], &cfg->threshold_db)) {
                fprintf(stderr, "ERROR: --threshold-db requires a numeric value\n");
                return false;
            }
        } else if (strcmp(arg, "--passband-hz") == 0) {
            if (++i >= argc || !parse_double_value(argv[i], &cfg->passband_hz)) {
                fprintf(stderr, "ERROR: --passband-hz requires a numeric value\n");
                return false;
            }
        } else if (strcmp(arg, "--if-offset-hz") == 0) {
            if (++i >= argc || !parse_double_value(argv[i], &cfg->if_offset_hz)) {
                fprintf(stderr, "ERROR: --if-offset-hz requires a numeric value\n");
                return false;
            }
        } else if (strcmp(arg, "--dc-exclude-hz") == 0) {
            if (++i >= argc || !parse_double_value(argv[i], &cfg->dc_exclude_hz)) {
                fprintf(stderr, "ERROR: --dc-exclude-hz requires a numeric value\n");
                return false;
            }
        } else if (strcmp(arg, "--settle-ms") == 0) {
            if (++i >= argc || !parse_int_value(argv[i], &cfg->settle_ms)) {
                fprintf(stderr, "ERROR: --settle-ms requires an integer value\n");
                return false;
            }
        } else if (strcmp(arg, "--delay-ms") == 0) {
            if (++i >= argc || !parse_int_value(argv[i], &cfg->delay_ms)) {
                fprintf(stderr, "ERROR: --delay-ms requires an integer value\n");
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

    if (!cfg->input_file) {
        fprintf(stderr, "ERROR: --in is required\n");
        return false;
    }

    if (cfg->sample_rate_hz <= 0 || cfg->averages <= 0 || cfg->cycles <= 0) {
        fprintf(stderr, "ERROR: rate, averages, and cycles must be greater than zero\n");
        return false;
    }

    if (cfg->bandwidth_hz <= 0) {
        cfg->bandwidth_hz = cfg->sample_rate_hz;
    }

    if (cfg->fft_size < 256 || !is_power_of_two(cfg->fft_size)) {
        fprintf(stderr, "ERROR: FFT size must be a power of two and at least 256\n");
        return false;
    }

    if (cfg->if_offset_hz <= 0.0 || cfg->passband_hz <= 0.0) {
        fprintf(stderr, "ERROR: IF offset and passband must be greater than zero\n");
        return false;
    }

    if (cfg->if_offset_hz + cfg->passband_hz > ((double)cfg->sample_rate_hz / 2.0)) {
        fprintf(stderr, "ERROR: IF offset plus passband is too close to Nyquist for this sample rate\n");
        return false;
    }

    if (cfg->settle_ms < 0 || cfg->delay_ms < 0 || cfg->dc_exclude_hz < 0.0) {
        fprintf(stderr, "ERROR: settle, delay, and DC exclude must not be negative\n");
        return false;
    }

    cfg->buffer_samples = cfg->fft_size;

    if (strcmp(cfg->gain_mode, "manual") == 0) {
        cfg->use_manual_gain = true;
    }

    return true;
}

static bool append_freq(freq_list_t *list, double freq_hz)
{
    if (freq_hz <= 0.0) {
        return true;
    }

    if (list->count >= list->capacity) {
        size_t new_capacity = list->capacity == 0 ? 64 : list->capacity * 2;
        double *new_freqs = (double *)realloc(list->freqs_hz, new_capacity * sizeof(double));

        if (!new_freqs) {
            return false;
        }

        list->freqs_hz = new_freqs;
        list->capacity = new_capacity;
    }

    list->freqs_hz[list->count++] = freq_hz;
    return true;
}

static bool load_frequencies(const char *filename, freq_list_t *list)
{
    FILE *f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "ERROR: Could not open frequency list: %s\n", filename);
        return false;
    }

    char line[1024];
    long line_number = 0;

    while (fgets(line, sizeof(line), f)) {
        line_number++;
        line[strcspn(line, "\r\n")] = '\0';

        if (line[0] == '\0') {
            continue;
        }

        if (strstr(line, "group_center_hz") || strstr(line, "center_hz")) {
            continue;
        }

        char *first = line;
        char *comma = strchr(line, ',');

        if (comma) {
            *comma = '\0';
        }

        char *end = NULL;
        errno = 0;
        double freq = strtod(first, &end);

        if (errno != 0 || end == first || freq <= 0.0) {
            fprintf(stderr, "WARNING: skipping frequency line %ld\n", line_number);
            continue;
        }

        if (!append_freq(list, freq)) {
            fclose(f);
            return false;
        }
    }

    fclose(f);
    return true;
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

static double bin_offset_hz(size_t bin, size_t fft_size, double sample_rate)
{
    if (bin < fft_size / 2) {
        return ((double)bin * sample_rate) / (double)fft_size;
    }

    return (((double)bin - (double)fft_size) * sample_rate) / (double)fft_size;
}

static bool in_range(double value, double center, double width_hz)
{
    return fabs(value - center) <= (width_hz / 2.0);
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
    double *power_db,
    double *noise_values,
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
    free(noise_values);
}

int main(int argc, char **argv)
{
    app_config_t cfg;

    if (!parse_args(argc, argv, &cfg)) {
        print_usage(argv[0]);
        return 1;
    }

    freq_list_t freqs;
    memset(&freqs, 0, sizeof(freqs));

    if (!load_frequencies(cfg.input_file, &freqs)) {
        free(freqs.freqs_hz);
        return 1;
    }

    if (freqs.count == 0) {
        fprintf(stderr, "ERROR: No frequencies loaded from %s\n", cfg.input_file);
        free(freqs.freqs_hz);
        return 1;
    }

    printf("\n");
    printf("Pluto+ Activity Monitor\n");
    printf("-----------------------\n");
    printf("URI:              %s\n", cfg.uri);
    printf("Input file:       %s\n", cfg.input_file);
    printf("Frequencies:      %zu\n", freqs.count);
    printf("CSV log:          %s\n", cfg.csv_file);
    printf("Sample rate:      %lld Hz\n", cfg.sample_rate_hz);
    printf("RF bandwidth:     %lld Hz\n", cfg.bandwidth_hz);
    printf("FFT size:         %zu\n", cfg.fft_size);
    printf("Averages:         %d\n", cfg.averages);
    printf("Cycles:           %d\n", cfg.cycles);
    printf("Threshold:        %.2f dB\n", cfg.threshold_db);
    printf("Passband:         %.1f Hz\n", cfg.passband_hz);
    printf("IF offset:        %.1f Hz\n", cfg.if_offset_hz);
    printf("DC exclude:       %.1f Hz\n", cfg.dc_exclude_hz);
    printf("Gain mode:        %s\n", cfg.gain_mode);

    if (cfg.use_manual_gain) {
        printf("Manual gain:      %lld dB\n", cfg.manual_gain_db);
    }

    printf("\n");

    struct iio_context *ctx = NULL;
    struct iio_buffer *rxbuf = NULL;
    FILE *csv = NULL;

    fftw_complex *fft_in = NULL;
    fftw_complex *fft_out = NULL;
    fftw_plan plan = NULL;

    double *avg_power = NULL;
    double *power_db = NULL;
    double *noise_values = NULL;

    ctx = iio_create_context_from_uri(cfg.uri);
    if (!ctx) {
        fprintf(stderr, "ERROR: Could not open IIO context: %s\n", cfg.uri);
        free(freqs.freqs_hz);
        return 1;
    }

    struct iio_device *phy = iio_context_find_device(ctx, "ad9361-phy");
    struct iio_device *rxdev = iio_context_find_device(ctx, "cf-ad9361-lpc");

    if (!phy || !rxdev) {
        fprintf(stderr, "ERROR: Missing ad9361-phy or cf-ad9361-lpc\n");
        cleanup(rxbuf, ctx, plan, fft_in, fft_out, avg_power, power_db, noise_values, csv);
        free(freqs.freqs_hz);
        return 1;
    }

    struct iio_channel *rx0_i = iio_device_find_channel(rxdev, "voltage0", false);
    struct iio_channel *rx0_q = iio_device_find_channel(rxdev, "voltage1", false);
    struct iio_channel *phy_rx0 = iio_device_find_channel(phy, "voltage0", false);
    struct iio_channel *rx_lo = iio_device_find_channel(phy, "altvoltage0", true);

    if (!rx0_i || !rx0_q || !phy_rx0 || !rx_lo) {
        fprintf(stderr, "ERROR: Missing required Pluto-compatible channels\n");
        cleanup(rxbuf, ctx, plan, fft_in, fft_out, avg_power, power_db, noise_values, csv);
        free(freqs.freqs_hz);
        return 1;
    }

    printf("Configuring baseband...\n");

    int rate_ret = ad9361_set_bb_rate(phy, (unsigned long)cfg.sample_rate_hz);
    if (rate_ret < 0) {
        fprintf(stderr, "ERROR: ad9361_set_bb_rate(%lld) failed, ret=%d\n", cfg.sample_rate_hz, rate_ret);
        cleanup(rxbuf, ctx, plan, fft_in, fft_out, avg_power, power_db, noise_values, csv);
        free(freqs.freqs_hz);
        return 1;
    }

    write_attr_ll(phy_rx0, "rf_bandwidth", cfg.bandwidth_hz);
    write_attr_str(phy_rx0, "gain_control_mode", cfg.gain_mode);

    if (cfg.use_manual_gain) {
        write_attr_ll(phy_rx0, "hardwaregain", cfg.manual_gain_db);
    }

    iio_channel_enable(rx0_i);
    iio_channel_enable(rx0_q);

    rxbuf = iio_device_create_buffer(rxdev, cfg.buffer_samples, false);
    if (!rxbuf) {
        fprintf(stderr, "ERROR: Could not create RX buffer\n");
        cleanup(rxbuf, ctx, plan, fft_in, fft_out, avg_power, power_db, noise_values, csv);
        free(freqs.freqs_hz);
        return 1;
    }

    fft_in = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * cfg.fft_size);
    fft_out = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * cfg.fft_size);
    avg_power = (double *)calloc(cfg.fft_size, sizeof(double));
    power_db = (double *)calloc(cfg.fft_size, sizeof(double));
    noise_values = (double *)calloc(cfg.fft_size, sizeof(double));

    if (!fft_in || !fft_out || !avg_power || !power_db || !noise_values) {
        fprintf(stderr, "ERROR: Could not allocate FFT buffers\n");
        cleanup(rxbuf, ctx, plan, fft_in, fft_out, avg_power, power_db, noise_values, csv);
        free(freqs.freqs_hz);
        return 1;
    }

    plan = fftw_plan_dft_1d((int)cfg.fft_size, fft_in, fft_out, FFTW_FORWARD, FFTW_ESTIMATE);
    if (!plan) {
        fprintf(stderr, "ERROR: Could not create FFTW plan\n");
        cleanup(rxbuf, ctx, plan, fft_in, fft_out, avg_power, power_db, noise_values, csv);
        free(freqs.freqs_hz);
        return 1;
    }

    csv = fopen(cfg.csv_file, "w");
    if (!csv) {
        fprintf(stderr, "ERROR: Could not open CSV log: %s\n", cfg.csv_file);
        cleanup(rxbuf, ctx, plan, fft_in, fft_out, avg_power, power_db, noise_values, csv);
        free(freqs.freqs_hz);
        return 1;
    }

    fprintf(csv, "timestamp,cycle,target_hz,tune_lo_hz,if_offset_hz,best_offset_hz,best_freq_hz,power_db,noise_floor_db,snr_db,active\n");

    printf("Starting monitor...\n\n");

    for (int cycle = 1; cycle <= cfg.cycles; cycle++) {
        printf("Cycle %d / %d\n", cycle, cfg.cycles);

        for (size_t fi = 0; fi < freqs.count; fi++) {
            double target_hz = freqs.freqs_hz[fi];
            long long tune_lo_hz = (long long)(target_hz - cfg.if_offset_hz + 0.5);

            if (tune_lo_hz <= 0) {
                fprintf(stderr, "WARNING: skipping %.3f Hz because LO would be invalid\n", target_hz);
                continue;
            }

            write_attr_ll(rx_lo, "frequency", tune_lo_hz);
            sleep_ms(cfg.settle_ms);

            /*
               Discard one stale buffer after retune.
            */
            iio_buffer_refill(rxbuf);

            memset(avg_power, 0, sizeof(double) * cfg.fft_size);

            for (int avg = 0; avg < cfg.averages; avg++) {
                ssize_t nbytes = iio_buffer_refill(rxbuf);

                if (nbytes < 0) {
                    fprintf(stderr, "ERROR: RX buffer refill failed: %zd\n", nbytes);
                    cleanup(rxbuf, ctx, plan, fft_in, fft_out, avg_power, power_db, noise_values, csv);
                    free(freqs.freqs_hz);
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

            double best_power_db = -1e300;
            double best_offset_hz = 0.0;
            size_t noise_count = 0;

            for (size_t k = 0; k < cfg.fft_size; k++) {
                avg_power[k] /= (double)cfg.averages;
                power_db[k] = 10.0 * log10(avg_power[k] + 1e-30);

                double offset_hz = bin_offset_hz(k, cfg.fft_size, (double)cfg.sample_rate_hz);

                bool is_signal_bin = in_range(offset_hz, cfg.if_offset_hz, cfg.passband_hz);
                bool is_dc_bin = fabs(offset_hz) <= cfg.dc_exclude_hz;

                if (is_signal_bin) {
                    if (power_db[k] > best_power_db) {
                        best_power_db = power_db[k];
                        best_offset_hz = offset_hz;
                    }
                } else if (!is_dc_bin) {
                    noise_values[noise_count++] = power_db[k];
                }
            }

            if (noise_count == 0) {
                fprintf(stderr, "WARNING: no noise bins available for %.3f Hz\n", target_hz);
                continue;
            }

            qsort(noise_values, noise_count, sizeof(double), compare_double);
            double noise_floor_db = noise_values[noise_count / 2];

            double snr_db = best_power_db - noise_floor_db;
            bool active = snr_db >= cfg.threshold_db;
            double best_freq_hz = (double)tune_lo_hz + best_offset_hz;

            char ts[64];
            now_string(ts, sizeof(ts));

            printf("  %10.6f MHz  SNR %7.2f dB  %-6s  best %.6f MHz\n",
                   target_hz / 1e6,
                   snr_db,
                   active ? "ACTIVE" : "quiet",
                   best_freq_hz / 1e6);

            fprintf(csv,
                    "%s,%d,%.3f,%lld,%.3f,%.3f,%.3f,%.2f,%.2f,%.2f,%d\n",
                    ts,
                    cycle,
                    target_hz,
                    tune_lo_hz,
                    cfg.if_offset_hz,
                    best_offset_hz,
                    best_freq_hz,
                    best_power_db,
                    noise_floor_db,
                    snr_db,
                    active ? 1 : 0);

            fflush(csv);

            if (cfg.delay_ms > 0) {
                sleep_ms(cfg.delay_ms);
            }
        }

        printf("\n");
    }

    printf("Activity monitor complete.\n");
    printf("CSV log written: %s\n", cfg.csv_file);

    cleanup(rxbuf, ctx, plan, fft_in, fft_out, avg_power, power_db, noise_values, csv);
    free(freqs.freqs_hz);

    return 0;
}
