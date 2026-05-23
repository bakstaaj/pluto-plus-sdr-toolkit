#include <iio.h>
#include <ad9361.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *uri;
    const char *out_file;

    long long freq_hz;
    long long sample_rate_hz;
    long long bandwidth_hz;
    long long manual_gain_db;

    double seconds;
    size_t buffer_samples;

    const char *gain_mode;
    bool use_manual_gain;
} app_config_t;

static void print_usage(const char *prog)
{
    printf("\n");
    printf("Pluto+ IQ Recorder - single RX Pluto-compatible mode\n");
    printf("\n");
    printf("Usage:\n");
    printf("  %s [options]\n", prog);
    printf("\n");
    printf("Options:\n");
    printf("  --uri <uri>             IIO URI, default ip:192.168.2.1\n");
    printf("  --freq <hz>             RX center frequency in Hz, default 100000000\n");
    printf("  --rate <hz>             Sample rate in Hz, default 1000000\n");
    printf("  --bw <hz>               RF bandwidth in Hz, default equals sample rate\n");
    printf("  --seconds <sec>         Capture duration, default 5\n");
    printf("  --buffer <samples>      RX buffer size in complex samples, default 16384\n");
    printf("  --gain-mode <mode>      slow_attack, fast_attack, manual, default slow_attack\n");
    printf("  --gain-db <db>          Manual RX gain in dB, only used with manual gain mode\n");
    printf("  --out <file>            Output file, default capture_iq_s16le.iq\n");
    printf("  --help                  Show this help\n");
    printf("\n");
    printf("Output format:\n");
    printf("  Raw interleaved signed 16-bit little-endian IQ\n");
    printf("  I0 Q0 I1 Q1 I2 Q2 ...\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s --freq 100000000 --rate 1000000 --seconds 5 --out fm_test.iq\n", prog);
    printf("  %s --freq 145000000 --rate 1000000 --seconds 10 --gain-mode slow_attack --out vhf.iq\n", prog);
    printf("  %s --freq 915000000 --rate 2000000 --bw 2000000 --seconds 15 --out ism_915.iq\n", prog);
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

static bool parse_args(int argc, char **argv, app_config_t *cfg)
{
    cfg->uri = "ip:192.168.2.1";
    cfg->out_file = "capture_iq_s16le.iq";

    cfg->freq_hz = 100000000LL;
    cfg->sample_rate_hz = 1000000LL;
    cfg->bandwidth_hz = 0;
    cfg->manual_gain_db = 30;

    cfg->seconds = 5.0;
    cfg->buffer_samples = 16384;

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
        } else if (strcmp(arg, "--seconds") == 0) {
            if (++i >= argc || !parse_double_value(argv[i], &cfg->seconds)) {
                fprintf(stderr, "ERROR: --seconds requires a numeric value\n");
                return false;
            }
        } else if (strcmp(arg, "--buffer") == 0) {
            if (++i >= argc || !parse_size_value(argv[i], &cfg->buffer_samples)) {
                fprintf(stderr, "ERROR: --buffer requires an integer sample count\n");
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
        } else if (strcmp(arg, "--out") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "ERROR: --out requires a filename\n");
                return false;
            }
            cfg->out_file = argv[i];
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

    if (cfg->seconds <= 0.0) {
        fprintf(stderr, "ERROR: seconds must be greater than zero\n");
        return false;
    }

    if (cfg->buffer_samples == 0) {
        fprintf(stderr, "ERROR: buffer samples must be greater than zero\n");
        return false;
    }

    if (cfg->bandwidth_hz <= 0) {
        cfg->bandwidth_hz = cfg->sample_rate_hz;
    }

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
    FILE *out,
    struct iio_context *ctx)
{
    if (out) {
        fclose(out);
    }

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

    printf("\n");
    printf("Pluto+ IQ Recorder\n");
    printf("------------------\n");
    printf("URI:              %s\n", cfg.uri);
    printf("Frequency:        %lld Hz\n", cfg.freq_hz);
    printf("Sample rate:      %lld Hz\n", cfg.sample_rate_hz);
    printf("RF bandwidth:     %lld Hz\n", cfg.bandwidth_hz);
    printf("Duration:         %.3f seconds\n", cfg.seconds);
    printf("Buffer samples:   %zu complex samples\n", cfg.buffer_samples);
    printf("Gain mode:        %s\n", cfg.gain_mode);

    if (cfg.use_manual_gain) {
        printf("Manual gain:      %lld dB\n", cfg.manual_gain_db);
    }

    printf("Output file:      %s\n", cfg.out_file);
    printf("\n");

    uint64_t target_samples = (uint64_t)(cfg.seconds * (double)cfg.sample_rate_hz + 0.5);

    if (target_samples == 0) {
        fprintf(stderr, "ERROR: target sample count is zero\n");
        return 1;
    }

    printf("Target samples:   %" PRIu64 " complex samples\n", target_samples);

    struct iio_context *ctx = NULL;
    struct iio_buffer *rxbuf = NULL;
    FILE *out = NULL;

    ctx = iio_create_context_from_uri(cfg.uri);
    if (!ctx) {
        fprintf(stderr, "ERROR: Could not open IIO context: %s\n", cfg.uri);
        fprintf(stderr, "Try this first:\n");
        fprintf(stderr, "  iio_info -u %s\n", cfg.uri);
        cleanup(rxbuf, out, ctx);
        return 1;
    }

    struct iio_device *phy = iio_context_find_device(ctx, "ad9361-phy");
    struct iio_device *rxdev = iio_context_find_device(ctx, "cf-ad9361-lpc");

    if (!phy) {
        fprintf(stderr, "ERROR: Could not find ad9361-phy\n");
        cleanup(rxbuf, out, ctx);
        return 1;
    }

    if (!rxdev) {
        fprintf(stderr, "ERROR: Could not find cf-ad9361-lpc RX buffer device\n");
        cleanup(rxbuf, out, ctx);
        return 1;
    }

    /*
       Single-RX Pluto-compatible buffered RX:
       voltage0 = RX0 I
       voltage1 = RX0 Q
    */
    struct iio_channel *rx0_i = iio_device_find_channel(rxdev, "voltage0", false);
    struct iio_channel *rx0_q = iio_device_find_channel(rxdev, "voltage1", false);

    if (!rx0_i || !rx0_q) {
        fprintf(stderr, "ERROR: Could not find RX0 I/Q scan channels voltage0 and voltage1\n");
        cleanup(rxbuf, out, ctx);
        return 1;
    }

    /*
       Control-side PHY channels:
       voltage0 input = RX0 controls
       altvoltage0 output = RX LO on Pluto-compatible images
    */
    struct iio_channel *phy_rx0 = iio_device_find_channel(phy, "voltage0", false);
    struct iio_channel *rx_lo = iio_device_find_channel(phy, "altvoltage0", true);

    if (!phy_rx0) {
        fprintf(stderr, "ERROR: Could not find PHY RX0 control channel voltage0\n");
        cleanup(rxbuf, out, ctx);
        return 1;
    }

    if (!rx_lo) {
        fprintf(stderr, "ERROR: Could not find RX LO channel altvoltage0\n");
        cleanup(rxbuf, out, ctx);
        return 1;
    }

    printf("\nConfiguring Pluto+...\n");

    printf("Setting AD936x baseband sample rate...\n");

    int rate_ret = ad9361_set_bb_rate(phy, (unsigned long)cfg.sample_rate_hz);
    if (rate_ret < 0) {
       fprintf(stderr, "ERROR: ad9361_set_bb_rate(%lld) failed, ret=%d\n",
            cfg.sample_rate_hz,
            rate_ret);
       cleanup(rxbuf, out, ctx);
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
        fprintf(stderr, "Possible causes:\n");
        fprintf(stderr, "  - another program is using the Pluto+\n");
        fprintf(stderr, "  - RX channels were not enabled\n");
        fprintf(stderr, "  - wrong RX device name\n");
        cleanup(rxbuf, out, ctx);
        return 1;
    }

    out = fopen(cfg.out_file, "wb");
    if (!out) {
        fprintf(stderr, "ERROR: Could not open output file: %s\n", cfg.out_file);
        cleanup(rxbuf, out, ctx);
        return 1;
    }

    printf("Starting capture...\n\n");

    uint64_t written_samples = 0;
    uint64_t next_report = (uint64_t)cfg.sample_rate_hz;

    while (written_samples < target_samples) {
        ssize_t nbytes = iio_buffer_refill(rxbuf);

        if (nbytes < 0) {
            fprintf(stderr, "\nERROR: RX buffer refill failed: %zd\n", nbytes);
            cleanup(rxbuf, out, ctx);
            return 1;
        }

        char *p_i = (char *)iio_buffer_first(rxbuf, rx0_i);
        char *p_q = (char *)iio_buffer_first(rxbuf, rx0_q);
        char *p_end = (char *)iio_buffer_end(rxbuf);
        ptrdiff_t p_inc = iio_buffer_step(rxbuf);

        for (; p_i < p_end && p_q < p_end; p_i += p_inc, p_q += p_inc) {
            if (written_samples >= target_samples) {
                break;
            }

            int16_t i_sample = 0;
            int16_t q_sample = 0;

            memcpy(&i_sample, p_i, sizeof(int16_t));
            memcpy(&q_sample, p_q, sizeof(int16_t));

            if (fwrite(&i_sample, sizeof(int16_t), 1, out) != 1 ||
                fwrite(&q_sample, sizeof(int16_t), 1, out) != 1) {
                fprintf(stderr, "\nERROR: Failed writing IQ data to output file\n");
                cleanup(rxbuf, out, ctx);
                return 1;
            }

            written_samples++;

            if (written_samples >= next_report) {
                double pct = 100.0 * (double)written_samples / (double)target_samples;
                printf("Captured %" PRIu64 " / %" PRIu64 " samples, %.1f%%\n",
                       written_samples,
                       target_samples,
                       pct);

                next_report += (uint64_t)cfg.sample_rate_hz;
            }
        }
    }

    printf("\nCapture complete.\n");
    printf("Complex samples written: %" PRIu64 "\n", written_samples);
    printf("Bytes per complex sample: 4\n");
    printf("Expected file size:      %" PRIu64 " bytes\n", written_samples * 4ULL);
    printf("Output file:             %s\n", cfg.out_file);

    cleanup(rxbuf, out, ctx);
    return 0;
}
