#include <iio.h>
#include <ad9361.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
   pluto_audio_monitor.c

   First audio/demod milestone for the Pluto+ SDR Windows Toolkit.

   Current supported mode:
     nfm  - narrow FM demodulation to 16-bit PCM WAV

   Good first test target:
     NOAA weather radio:
       162.400 MHz
       162.425 MHz
       162.450 MHz
       162.475 MHz
       162.500 MHz
       162.525 MHz
       162.550 MHz

   Example:
     ./build/native/pluto_audio_monitor.exe \
       --mode nfm \
       --freq 162550000 \
       --rate 960000 \
       --audio-rate 48000 \
       --seconds 30 \
       --wav noaa.wav

   Notes:
     - Defaults assume Pluto+ coverage down to at least 70 MHz.
     - This is intentionally simple and self-contained.
     - FM demod is phase-difference demodulation.
     - Audio is low-pass filtered and written as mono 16-bit PCM WAV.
*/

#define PI_CONST 3.14159265358979323846

typedef struct {
    const char *uri;
    const char *mode;
    const char *wav_path;

    long long freq_hz;
    long long sample_rate_hz;
    long long bandwidth_hz;
    long long manual_gain_db;

    int audio_rate_hz;
    int seconds;
    int buffer_samples;

    double audio_lowpass_hz;
    double volume;

    const char *gain_mode;
    bool use_manual_gain;
} app_config_t;

typedef struct {
    FILE *fp;
    uint32_t sample_rate;
    uint32_t samples_written;
} wav_writer_t;

static void print_usage(const char *prog)
{
    printf("\n");
    printf("Pluto+ Audio Monitor\n");
    printf("\n");
    printf("Usage:\n");
    printf("  %s --mode nfm --freq <hz> --wav <file.wav> [options]\n", prog);
    printf("\n");
    printf("Options:\n");
    printf("  --uri <uri>             IIO URI, default ip:192.168.2.1\n");
    printf("  --mode <mode>           Demod mode: nfm, default nfm\n");
    printf("  --freq <hz>             RX center frequency Hz, default 162550000\n");
    printf("  --rate <hz>             SDR sample rate Hz, default 960000\n");
    printf("  --bw <hz>               RF bandwidth Hz, default 200000\n");
    printf("  --audio-rate <hz>       WAV audio sample rate, default 48000\n");
    printf("  --audio-lowpass-hz <hz> Audio low-pass cutoff, default 5000\n");
    printf("  --seconds <n>           Capture duration seconds, default 30\n");
    printf("  --wav <file>            Output WAV file, default noaa.wav\n");
    printf("  --volume <value>        Audio gain multiplier, default 3.0\n");
    printf("  --buffer-samples <n>    RX buffer samples, default 4096\n");
    printf("  --gain-mode <mode>      slow_attack, fast_attack, manual, default slow_attack\n");
    printf("  --gain-db <db>          Manual RX gain dB, implies manual gain mode\n");
    printf("  --help                  Show this help\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s --mode nfm --freq 162550000 --seconds 30 --wav noaa.wav\n", prog);
    printf("  %s --mode nfm --freq 146520000 --seconds 20 --wav 2m_call.wav\n", prog);
    printf("  %s --mode nfm --freq 162550000 --gain-mode manual --gain-db 40 --wav noaa.wav\n", prog);
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

static bool parse_args(int argc, char **argv, app_config_t *cfg)
{
    cfg->uri = "ip:192.168.2.1";
    cfg->mode = "nfm";
    cfg->wav_path = "noaa.wav";

    cfg->freq_hz = 162550000LL;
    cfg->sample_rate_hz = 960000LL;
    cfg->bandwidth_hz = 200000LL;
    cfg->manual_gain_db = 40;

    cfg->audio_rate_hz = 48000;
    cfg->seconds = 30;
    cfg->buffer_samples = 4096;

    cfg->audio_lowpass_hz = 5000.0;
    cfg->volume = 3.0;

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
        } else if (strcmp(arg, "--mode") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "ERROR: --mode requires a value\n");
                return false;
            }
            cfg->mode = argv[i];
        } else if (strcmp(arg, "--freq") == 0) {
            if (++i >= argc || !parse_ll(argv[i], &cfg->freq_hz)) {
                fprintf(stderr, "ERROR: --freq requires integer Hz\n");
                return false;
            }
        } else if (strcmp(arg, "--rate") == 0) {
            if (++i >= argc || !parse_ll(argv[i], &cfg->sample_rate_hz)) {
                fprintf(stderr, "ERROR: --rate requires integer Hz\n");
                return false;
            }
        } else if (strcmp(arg, "--bw") == 0) {
            if (++i >= argc || !parse_ll(argv[i], &cfg->bandwidth_hz)) {
                fprintf(stderr, "ERROR: --bw requires integer Hz\n");
                return false;
            }
        } else if (strcmp(arg, "--audio-rate") == 0) {
            if (++i >= argc || !parse_int_value(argv[i], &cfg->audio_rate_hz)) {
                fprintf(stderr, "ERROR: --audio-rate requires integer Hz\n");
                return false;
            }
        } else if (strcmp(arg, "--audio-lowpass-hz") == 0) {
            if (++i >= argc || !parse_double_value(argv[i], &cfg->audio_lowpass_hz)) {
                fprintf(stderr, "ERROR: --audio-lowpass-hz requires numeric Hz\n");
                return false;
            }
        } else if (strcmp(arg, "--seconds") == 0) {
            if (++i >= argc || !parse_int_value(argv[i], &cfg->seconds)) {
                fprintf(stderr, "ERROR: --seconds requires integer seconds\n");
                return false;
            }
        } else if (strcmp(arg, "--wav") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "ERROR: --wav requires a filename\n");
                return false;
            }
            cfg->wav_path = argv[i];
        } else if (strcmp(arg, "--volume") == 0) {
            if (++i >= argc || !parse_double_value(argv[i], &cfg->volume)) {
                fprintf(stderr, "ERROR: --volume requires a numeric value\n");
                return false;
            }
        } else if (strcmp(arg, "--buffer-samples") == 0) {
            if (++i >= argc || !parse_int_value(argv[i], &cfg->buffer_samples)) {
                fprintf(stderr, "ERROR: --buffer-samples requires integer samples\n");
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
                fprintf(stderr, "ERROR: --gain-db requires integer dB\n");
                return false;
            }
            cfg->gain_mode = "manual";
            cfg->use_manual_gain = true;
        } else {
            fprintf(stderr, "ERROR: Unknown option: %s\n", arg);
            return false;
        }
    }

    if (strcmp(cfg->mode, "nfm") != 0) {
        fprintf(stderr, "ERROR: only --mode nfm is supported in this first audio milestone\n");
        return false;
    }

    if (cfg->freq_hz <= 0 || cfg->sample_rate_hz <= 0 || cfg->bandwidth_hz <= 0) {
        fprintf(stderr, "ERROR: frequency, sample rate, and bandwidth must be positive\n");
        return false;
    }

    if (cfg->audio_rate_hz <= 0 || cfg->seconds <= 0 || cfg->buffer_samples <= 0) {
        fprintf(stderr, "ERROR: audio rate, seconds, and buffer samples must be positive\n");
        return false;
    }

    if (cfg->audio_lowpass_hz <= 0.0 || cfg->audio_lowpass_hz >= ((double)cfg->audio_rate_hz / 2.0)) {
        fprintf(stderr, "ERROR: audio low-pass must be > 0 and < audio Nyquist\n");
        return false;
    }

    if (cfg->volume <= 0.0) {
        fprintf(stderr, "ERROR: volume must be positive\n");
        return false;
    }

    return true;
}

static void write_le_u16(FILE *fp, uint16_t v)
{
    fputc((int)(v & 0xff), fp);
    fputc((int)((v >> 8) & 0xff), fp);
}

static void write_le_u32(FILE *fp, uint32_t v)
{
    fputc((int)(v & 0xff), fp);
    fputc((int)((v >> 8) & 0xff), fp);
    fputc((int)((v >> 16) & 0xff), fp);
    fputc((int)((v >> 24) & 0xff), fp);
}

static bool wav_open(wav_writer_t *wav, const char *path, uint32_t sample_rate)
{
    memset(wav, 0, sizeof(*wav));

    wav->fp = fopen(path, "wb");
    if (!wav->fp) {
        fprintf(stderr, "ERROR: could not open WAV file for writing: %s\n", path);
        return false;
    }

    wav->sample_rate = sample_rate;
    wav->samples_written = 0;

    /*
       Placeholder 44-byte WAV header. Sizes are updated in wav_close().
    */
    fwrite("RIFF", 1, 4, wav->fp);
    write_le_u32(wav->fp, 0);
    fwrite("WAVE", 1, 4, wav->fp);

    fwrite("fmt ", 1, 4, wav->fp);
    write_le_u32(wav->fp, 16);
    write_le_u16(wav->fp, 1);
    write_le_u16(wav->fp, 1);
    write_le_u32(wav->fp, sample_rate);
    write_le_u32(wav->fp, sample_rate * 2);
    write_le_u16(wav->fp, 2);
    write_le_u16(wav->fp, 16);

    fwrite("data", 1, 4, wav->fp);
    write_le_u32(wav->fp, 0);

    return true;
}

static void wav_write_sample(wav_writer_t *wav, int16_t sample)
{
    if (!wav->fp) {
        return;
    }

    write_le_u16(wav->fp, (uint16_t)sample);
    wav->samples_written++;
}

static void wav_close(wav_writer_t *wav)
{
    if (!wav->fp) {
        return;
    }

    uint32_t data_bytes = wav->samples_written * 2;
    uint32_t riff_size = 36 + data_bytes;

    fseek(wav->fp, 4, SEEK_SET);
    write_le_u32(wav->fp, riff_size);

    fseek(wav->fp, 40, SEEK_SET);
    write_le_u32(wav->fp, data_bytes);

    fclose(wav->fp);
    wav->fp = NULL;
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

static int16_t float_to_s16(double x)
{
    if (x > 1.0) {
        x = 1.0;
    } else if (x < -1.0) {
        x = -1.0;
    }

    return (int16_t)lrint(x * 32767.0);
}

static void cleanup(struct iio_buffer *rxbuf, struct iio_context *ctx, wav_writer_t *wav)
{
    if (rxbuf) {
        iio_buffer_destroy(rxbuf);
    }

    if (ctx) {
        iio_context_destroy(ctx);
    }

    if (wav) {
        wav_close(wav);
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
    printf("Pluto+ Audio Monitor\n");
    printf("--------------------\n");
    printf("URI:              %s\n", cfg.uri);
    printf("Mode:             %s\n", cfg.mode);
    printf("Frequency:        %lld Hz\n", cfg.freq_hz);
    printf("Sample rate:      %lld Hz\n", cfg.sample_rate_hz);
    printf("RF bandwidth:     %lld Hz\n", cfg.bandwidth_hz);
    printf("Audio rate:       %d Hz\n", cfg.audio_rate_hz);
    printf("Audio low-pass:   %.1f Hz\n", cfg.audio_lowpass_hz);
    printf("Duration:         %d seconds\n", cfg.seconds);
    printf("WAV output:       %s\n", cfg.wav_path);
    printf("Gain mode:        %s\n", cfg.gain_mode);

    if (cfg.use_manual_gain) {
        printf("Manual gain:      %lld dB\n", cfg.manual_gain_db);
    }

    printf("\n");

    wav_writer_t wav;
    struct iio_context *ctx = NULL;
    struct iio_buffer *rxbuf = NULL;

    if (!wav_open(&wav, cfg.wav_path, (uint32_t)cfg.audio_rate_hz)) {
        return 1;
    }

    printf("Opening IIO context: %s\n", cfg.uri);
    ctx = iio_create_context_from_uri(cfg.uri);
    if (!ctx) {
        fprintf(stderr, "ERROR: Could not open IIO context: %s\n", cfg.uri);
        cleanup(rxbuf, ctx, &wav);
        return 1;
    }

    struct iio_device *phy = iio_context_find_device(ctx, "ad9361-phy");
    struct iio_device *rxdev = iio_context_find_device(ctx, "cf-ad9361-lpc");

    if (!phy || !rxdev) {
        fprintf(stderr, "ERROR: Missing ad9361-phy or cf-ad9361-lpc\n");
        cleanup(rxbuf, ctx, &wav);
        return 1;
    }

    struct iio_channel *rx0_i = iio_device_find_channel(rxdev, "voltage0", false);
    struct iio_channel *rx0_q = iio_device_find_channel(rxdev, "voltage1", false);
    struct iio_channel *phy_rx0 = iio_device_find_channel(phy, "voltage0", false);
    struct iio_channel *rx_lo = iio_device_find_channel(phy, "altvoltage0", true);

    if (!rx0_i || !rx0_q || !phy_rx0 || !rx_lo) {
        fprintf(stderr, "ERROR: Missing required Pluto-compatible RX channels\n");
        cleanup(rxbuf, ctx, &wav);
        return 1;
    }

    printf("Configuring SDR...\n");

    int rate_ret = ad9361_set_bb_rate(phy, (unsigned long)cfg.sample_rate_hz);
    if (rate_ret < 0) {
        fprintf(stderr, "ERROR: ad9361_set_bb_rate(%lld) failed, ret=%d\n", cfg.sample_rate_hz, rate_ret);
        cleanup(rxbuf, ctx, &wav);
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

    rxbuf = iio_device_create_buffer(rxdev, (size_t)cfg.buffer_samples, false);
    if (!rxbuf) {
        fprintf(stderr, "ERROR: Could not create RX buffer\n");
        cleanup(rxbuf, ctx, &wav);
        return 1;
    }

    printf("Discarding initial buffers...\n");
    for (int i = 0; i < 3; i++) {
        iio_buffer_refill(rxbuf);
    }

    printf("Recording audio...\n");

    const uint32_t target_audio_samples = (uint32_t)cfg.seconds * (uint32_t)cfg.audio_rate_hz;
    const double input_rate = (double)cfg.sample_rate_hz;
    const double output_rate = (double)cfg.audio_rate_hz;
    const double output_step = input_rate / output_rate;

    const double lp_alpha = 1.0 - exp((-2.0 * PI_CONST * cfg.audio_lowpass_hz) / input_rate);

    double prev_i = 1.0;
    double prev_q = 0.0;
    bool have_prev = false;

    double filt_prev = 0.0;
    double filt_current = 0.0;

    double input_index = 0.0;
    double next_output_index = 1.0;

    uint64_t raw_samples_processed = 0;

    while (wav.samples_written < target_audio_samples) {
        ssize_t nbytes = iio_buffer_refill(rxbuf);
        if (nbytes < 0) {
            fprintf(stderr, "ERROR: RX buffer refill failed: %zd\n", nbytes);
            cleanup(rxbuf, ctx, &wav);
            return 1;
        }

        char *p_i = (char *)iio_buffer_first(rxbuf, rx0_i);
        char *p_q = (char *)iio_buffer_first(rxbuf, rx0_q);
        char *p_end = (char *)iio_buffer_end(rxbuf);
        ptrdiff_t p_inc = iio_buffer_step(rxbuf);

        for (; p_i < p_end && p_q < p_end; p_i += p_inc, p_q += p_inc) {
            int16_t i_s16 = 0;
            int16_t q_s16 = 0;

            memcpy(&i_s16, p_i, sizeof(int16_t));
            memcpy(&q_s16, p_q, sizeof(int16_t));

            double i_now = (double)i_s16 / 32768.0;
            double q_now = (double)q_s16 / 32768.0;

            if (!have_prev) {
                prev_i = i_now;
                prev_q = q_now;
                have_prev = true;
                continue;
            }

            /*
               Phase difference FM demod:
                 dphi = atan2(cross, dot)
            */
            double cross = prev_i * q_now - prev_q * i_now;
            double dot = prev_i * i_now + prev_q * q_now;
            double dphi = atan2(cross, dot);

            prev_i = i_now;
            prev_q = q_now;

            /*
               Normalize NFM phase swing to audio-ish scale.
               The multiplier is intentionally conservative; user can change --volume.
            */
            double audio = dphi * cfg.volume;

            filt_prev = filt_current;
            filt_current = filt_current + lp_alpha * (audio - filt_current);

            raw_samples_processed++;
            input_index += 1.0;

            while (wav.samples_written < target_audio_samples && input_index >= next_output_index) {
                double frac = 1.0 - (input_index - next_output_index);
                if (frac < 0.0) {
                    frac = 0.0;
                } else if (frac > 1.0) {
                    frac = 1.0;
                }

                double interpolated = filt_prev + frac * (filt_current - filt_prev);
                wav_write_sample(&wav, float_to_s16(interpolated));

                next_output_index += output_step;
            }
        }

        if ((wav.samples_written % (uint32_t)cfg.audio_rate_hz) < 512) {
            printf("\rAudio samples: %u / %u", wav.samples_written, target_audio_samples);
            fflush(stdout);
        }
    }

    printf("\n");
    printf("Done.\n");
    printf("Raw IQ samples processed: %llu\n", (unsigned long long)raw_samples_processed);
    printf("Audio samples written:    %u\n", wav.samples_written);
    printf("WAV file:                 %s\n", cfg.wav_path);

    cleanup(rxbuf, ctx, &wav);
    return 0;
}
