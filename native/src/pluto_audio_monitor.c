#include <iio.h>
#include <ad9361.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*
   pluto_audio_monitor.c

   Audio monitor for the Pluto+ SDR Windows Toolkit.

   Supported modes:
     nfm   Narrow FM, useful for NOAA and 2m FM voice
     am    AM envelope demod, useful for VHF airband
     wbfm  Wide FM, useful for FM broadcast band tests

   Notes:
     - This is a simple WAV recorder, not a stereo/RDS broadcast receiver.
     - WBFM mode records mono broadcast FM audio using FM phase demod.
     - WBFM defaults are easier to validate on strong local FM stations.
*/

#define PI_CONST 3.14159265358979323846
#define MAX_PATH_TEXT 512

typedef struct {
    const char *name;
    long long freq_hz;
    const char *default_mode;
    long long default_rate_hz;
    long long default_bw_hz;
    double default_lowpass_hz;
    double default_volume;
} preset_t;

static const preset_t presets[] = {
    {"noaa1", 162400000LL, "nfm", 960000LL, 200000LL, 5000.0, 3.0},
    {"noaa2", 162425000LL, "nfm", 960000LL, 200000LL, 5000.0, 3.0},
    {"noaa3", 162450000LL, "nfm", 960000LL, 200000LL, 5000.0, 3.0},
    {"noaa4", 162475000LL, "nfm", 960000LL, 200000LL, 5000.0, 3.0},
    {"noaa5", 162500000LL, "nfm", 960000LL, 200000LL, 5000.0, 3.0},
    {"noaa6", 162525000LL, "nfm", 960000LL, 200000LL, 5000.0, 3.0},
    {"noaa7", 162550000LL, "nfm", 960000LL, 200000LL, 5000.0, 3.0},
    {"noaa-162550", 162550000LL, "nfm", 960000LL, 200000LL, 5000.0, 3.0},
    {"2m-call", 146520000LL, "nfm", 960000LL, 200000LL, 5000.0, 3.0},

    {"airband-118", 118000000LL, "am", 960000LL, 200000LL, 5000.0, 3.0},
    {"airband-120", 120000000LL, "am", 960000LL, 200000LL, 5000.0, 3.0},
    {"airband-1228", 122800000LL, "am", 960000LL, 200000LL, 5000.0, 3.0},
    {"airband-125", 125000000LL, "am", 960000LL, 200000LL, 5000.0, 3.0},
    {"airband-1275", 127500000LL, "am", 960000LL, 200000LL, 5000.0, 3.0},
    {"airband-130", 130000000LL, "am", 960000LL, 200000LL, 5000.0, 3.0},

    {"fm-88", 88000000LL, "wbfm", 2400000LL, 1800000LL, 15000.0, 0.8},
    {"fm-90", 90000000LL, "wbfm", 2400000LL, 1800000LL, 15000.0, 0.8},
    {"fm-94", 94000000LL, "wbfm", 2400000LL, 1800000LL, 15000.0, 0.8},
    {"fm-98", 98000000LL, "wbfm", 2400000LL, 1800000LL, 15000.0, 0.8},
    {"fm-100", 100000000LL, "wbfm", 2400000LL, 1800000LL, 15000.0, 0.8},
    {"fm-102", 102000000LL, "wbfm", 2400000LL, 1800000LL, 15000.0, 0.8},
    {"fm-104", 104000000LL, "wbfm", 2400000LL, 1800000LL, 15000.0, 0.8},
    {"fm-106", 106000000LL, "wbfm", 2400000LL, 1800000LL, 15000.0, 0.8}
};

typedef struct {
    const char *uri;
    const char *mode;
    const char *wav_path;
    const char *csv_path;
    const char *preset_name;

    char auto_wav_path[MAX_PATH_TEXT];

    long long freq_hz;
    long long sample_rate_hz;
    long long bandwidth_hz;
    long long manual_gain_db;

    int audio_rate_hz;
    int seconds;
    int buffer_samples;

    double audio_lowpass_hz;
    double volume;
    double deemphasis_us;

    double squelch_db;
    bool squelch_enabled;

    const char *gain_mode;
    bool use_manual_gain;

    bool mode_was_set;
    bool rate_was_set;
    bool bw_was_set;
    bool lowpass_was_set;
    bool volume_was_set;
} app_config_t;

typedef struct {
    FILE *fp;
    uint32_t sample_rate;
    uint32_t samples_written;
} wav_writer_t;

typedef struct {
    double rf_sum_db;
    double rf_min_db;
    double rf_max_db;

    double audio_sum_sq;
    double audio_peak;

    uint64_t raw_samples;
    uint64_t audio_samples;
    uint64_t squelch_open_raw_samples;
    uint64_t squelch_closed_raw_samples;

    uint32_t last_printed_second;
} stats_t;

static void print_usage(const char *prog)
{
    printf("\n");
    printf("Pluto+ Audio Monitor\n");
    printf("\n");
    printf("Usage:\n");
    printf("  %s [--mode nfm|am|wbfm] [--freq <hz> | --preset <name>] [options]\n", prog);
    printf("\n");
    printf("Options:\n");
    printf("  --uri <uri>             IIO URI, default ip:192.168.2.1\n");
    printf("  --mode <mode>           nfm, am, or wbfm. Presets choose a default mode.\n");
    printf("  --preset <name>         noaa7, airband-125, fm-100, etc.\n");
    printf("  --freq <hz>             RX center frequency Hz\n");
    printf("  --rate <hz>             SDR sample rate Hz\n");
    printf("  --bw <hz>               RF bandwidth Hz\n");
    printf("  --audio-rate <hz>       WAV audio sample rate, default 48000\n");
    printf("  --audio-lowpass-hz <hz> Audio low-pass cutoff\n");
    printf("  --deemphasis-us <us>    WBFM de-emphasis, default 75 us\n");
    printf("  --seconds <n>           Capture duration seconds, default 30\n");
    printf("  --wav <file>            Output WAV file, default auto timestamped name\n");
    printf("  --csv <file>            Append summary CSV row\n");
    printf("  --volume <value>        Audio gain multiplier\n");
    printf("  --squelch-db <dbfs>     RF squelch threshold dBFS, default -65\n");
    printf("  --squelch-off           Disable squelch\n");
    printf("  --buffer-samples <n>    RX buffer samples, default 8192\n");
    printf("  --gain-mode <mode>      slow_attack, fast_attack, manual, default slow_attack\n");
    printf("  --gain-db <db>          Manual RX gain dB, implies manual gain mode\n");
    printf("  --help                  Show this help\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s --preset noaa7 --seconds 30 --wav sessions/noaa.wav --csv sessions/audio_log.csv\n", prog);
    printf("  %s --mode am --preset airband-125 --seconds 60 --wav sessions/airband_am.wav --csv sessions/audio_log.csv\n", prog);
    printf("  %s --preset fm-100 --seconds 30 --wav sessions/fm100.wav --csv sessions/audio_log.csv\n", prog);
    printf("  %s --mode wbfm --freq 100000000 --seconds 30 --squelch-off --wav sessions/fm100_open.wav\n", prog);
    printf("\n");
}

static bool parse_ll(const char *text, long long *value)
{
    char *end = NULL;
    errno = 0;
    long long v = strtoll(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0') return false;
    *value = v;
    return true;
}

static bool parse_int_value(const char *text, int *value)
{
    char *end = NULL;
    errno = 0;
    long v = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0') return false;
    *value = (int)v;
    return true;
}

static bool parse_double_value(const char *text, double *value)
{
    char *end = NULL;
    errno = 0;
    double v = strtod(text, &end);
    if (errno != 0 || end == text || *end != '\0') return false;
    *value = v;
    return true;
}

static bool lookup_preset(const char *name, const preset_t **preset)
{
    size_t count = sizeof(presets) / sizeof(presets[0]);

    for (size_t i = 0; i < count; i++) {
        if (strcmp(name, presets[i].name) == 0) {
            *preset = &presets[i];
            return true;
        }
    }

    return false;
}

static void make_timestamp(char *out, size_t out_size)
{
    time_t now = time(NULL);
    struct tm tm_value;

#ifdef _WIN32
    localtime_s(&tm_value, &now);
#else
    localtime_r(&now, &tm_value);
#endif

    strftime(out, out_size, "%Y%m%d_%H%M%S", &tm_value);
}

static bool is_valid_mode(const char *mode)
{
    return strcmp(mode, "nfm") == 0 || strcmp(mode, "am") == 0 || strcmp(mode, "wbfm") == 0;
}

static void make_auto_wav_path(app_config_t *cfg)
{
    char timestamp[32];
    make_timestamp(timestamp, sizeof(timestamp));

    const char *prefix = "audio";
    if (cfg->preset_name && strlen(cfg->preset_name) > 0) prefix = cfg->preset_name;

    snprintf(cfg->auto_wav_path, sizeof(cfg->auto_wav_path),
             "%s_%s_%lld_%s.wav", prefix, cfg->mode, cfg->freq_hz, timestamp);

    cfg->wav_path = cfg->auto_wav_path;
}

static bool parse_args(int argc, char **argv, app_config_t *cfg)
{
    cfg->uri = "ip:192.168.2.1";
    cfg->mode = "nfm";
    cfg->wav_path = NULL;
    cfg->csv_path = NULL;
    cfg->preset_name = NULL;
    cfg->auto_wav_path[0] = '\0';

    cfg->freq_hz = 162550000LL;
    cfg->sample_rate_hz = 960000LL;
    cfg->bandwidth_hz = 200000LL;
    cfg->manual_gain_db = 40;

    cfg->audio_rate_hz = 48000;
    cfg->seconds = 30;
    cfg->buffer_samples = 8192;

    cfg->audio_lowpass_hz = 5000.0;
    cfg->volume = 3.0;
    cfg->deemphasis_us = 75.0;

    cfg->squelch_db = -65.0;
    cfg->squelch_enabled = true;

    cfg->gain_mode = "slow_attack";
    cfg->use_manual_gain = false;

    cfg->mode_was_set = false;
    cfg->rate_was_set = false;
    cfg->bw_was_set = false;
    cfg->lowpass_was_set = false;
    cfg->volume_was_set = false;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];

        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            print_usage(argv[0]);
            exit(0);
        } else if (strcmp(arg, "--uri") == 0) {
            if (++i >= argc) { fprintf(stderr, "ERROR: --uri requires a value\n"); return false; }
            cfg->uri = argv[i];
        } else if (strcmp(arg, "--mode") == 0) {
            if (++i >= argc) { fprintf(stderr, "ERROR: --mode requires a value\n"); return false; }
            cfg->mode = argv[i];
            cfg->mode_was_set = true;
        } else if (strcmp(arg, "--preset") == 0) {
            const preset_t *preset = NULL;
            if (++i >= argc) { fprintf(stderr, "ERROR: --preset requires a value\n"); return false; }
            cfg->preset_name = argv[i];

            if (!lookup_preset(cfg->preset_name, &preset)) {
                fprintf(stderr, "ERROR: unknown preset: %s\n", cfg->preset_name);
                return false;
            }

            cfg->freq_hz = preset->freq_hz;
            if (!cfg->mode_was_set) cfg->mode = preset->default_mode;
            if (!cfg->rate_was_set) cfg->sample_rate_hz = preset->default_rate_hz;
            if (!cfg->bw_was_set) cfg->bandwidth_hz = preset->default_bw_hz;
            if (!cfg->lowpass_was_set) cfg->audio_lowpass_hz = preset->default_lowpass_hz;
            if (!cfg->volume_was_set) cfg->volume = preset->default_volume;
        } else if (strcmp(arg, "--freq") == 0) {
            if (++i >= argc || !parse_ll(argv[i], &cfg->freq_hz)) { fprintf(stderr, "ERROR: --freq requires integer Hz\n"); return false; }
        } else if (strcmp(arg, "--rate") == 0) {
            if (++i >= argc || !parse_ll(argv[i], &cfg->sample_rate_hz)) { fprintf(stderr, "ERROR: --rate requires integer Hz\n"); return false; }
            cfg->rate_was_set = true;
        } else if (strcmp(arg, "--bw") == 0) {
            if (++i >= argc || !parse_ll(argv[i], &cfg->bandwidth_hz)) { fprintf(stderr, "ERROR: --bw requires integer Hz\n"); return false; }
            cfg->bw_was_set = true;
        } else if (strcmp(arg, "--audio-rate") == 0) {
            if (++i >= argc || !parse_int_value(argv[i], &cfg->audio_rate_hz)) { fprintf(stderr, "ERROR: --audio-rate requires integer Hz\n"); return false; }
        } else if (strcmp(arg, "--audio-lowpass-hz") == 0) {
            if (++i >= argc || !parse_double_value(argv[i], &cfg->audio_lowpass_hz)) { fprintf(stderr, "ERROR: --audio-lowpass-hz requires numeric Hz\n"); return false; }
            cfg->lowpass_was_set = true;
        } else if (strcmp(arg, "--deemphasis-us") == 0) {
            if (++i >= argc || !parse_double_value(argv[i], &cfg->deemphasis_us)) { fprintf(stderr, "ERROR: --deemphasis-us requires numeric microseconds\n"); return false; }
        } else if (strcmp(arg, "--seconds") == 0) {
            if (++i >= argc || !parse_int_value(argv[i], &cfg->seconds)) { fprintf(stderr, "ERROR: --seconds requires integer seconds\n"); return false; }
        } else if (strcmp(arg, "--wav") == 0) {
            if (++i >= argc) { fprintf(stderr, "ERROR: --wav requires a filename\n"); return false; }
            cfg->wav_path = argv[i];
        } else if (strcmp(arg, "--csv") == 0) {
            if (++i >= argc) { fprintf(stderr, "ERROR: --csv requires a filename\n"); return false; }
            cfg->csv_path = argv[i];
        } else if (strcmp(arg, "--volume") == 0) {
            if (++i >= argc || !parse_double_value(argv[i], &cfg->volume)) { fprintf(stderr, "ERROR: --volume requires a numeric value\n"); return false; }
            cfg->volume_was_set = true;
        } else if (strcmp(arg, "--squelch-db") == 0) {
            if (++i >= argc || !parse_double_value(argv[i], &cfg->squelch_db)) { fprintf(stderr, "ERROR: --squelch-db requires a numeric dBFS value\n"); return false; }
            cfg->squelch_enabled = true;
        } else if (strcmp(arg, "--squelch-off") == 0) {
            cfg->squelch_enabled = false;
        } else if (strcmp(arg, "--buffer-samples") == 0) {
            if (++i >= argc || !parse_int_value(argv[i], &cfg->buffer_samples)) { fprintf(stderr, "ERROR: --buffer-samples requires integer samples\n"); return false; }
        } else if (strcmp(arg, "--gain-mode") == 0) {
            if (++i >= argc) { fprintf(stderr, "ERROR: --gain-mode requires a value\n"); return false; }
            cfg->gain_mode = argv[i];
        } else if (strcmp(arg, "--gain-db") == 0) {
            if (++i >= argc || !parse_ll(argv[i], &cfg->manual_gain_db)) { fprintf(stderr, "ERROR: --gain-db requires integer dB\n"); return false; }
            cfg->gain_mode = "manual";
            cfg->use_manual_gain = true;
        } else {
            fprintf(stderr, "ERROR: Unknown option: %s\n", arg);
            return false;
        }
    }

    if (!is_valid_mode(cfg->mode)) { fprintf(stderr, "ERROR: --mode must be nfm, am, or wbfm\n"); return false; }
    if (cfg->freq_hz <= 0 || cfg->sample_rate_hz <= 0 || cfg->bandwidth_hz <= 0) { fprintf(stderr, "ERROR: frequency, sample rate, and bandwidth must be positive\n"); return false; }
    if (cfg->audio_rate_hz <= 0 || cfg->seconds <= 0 || cfg->buffer_samples <= 0) { fprintf(stderr, "ERROR: audio rate, seconds, and buffer samples must be positive\n"); return false; }
    if (cfg->audio_lowpass_hz <= 0.0 || cfg->audio_lowpass_hz >= ((double)cfg->audio_rate_hz / 2.0)) { fprintf(stderr, "ERROR: audio low-pass must be > 0 and < audio Nyquist\n"); return false; }
    if (cfg->volume <= 0.0) { fprintf(stderr, "ERROR: volume must be positive\n"); return false; }
    if (cfg->deemphasis_us <= 0.0) { fprintf(stderr, "ERROR: deemphasis must be positive\n"); return false; }

    if (!cfg->wav_path) make_auto_wav_path(cfg);
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
    if (!wav->fp) { fprintf(stderr, "ERROR: could not open WAV file: %s\n", path); return false; }
    wav->sample_rate = sample_rate;
    wav->samples_written = 0;

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
    if (!wav->fp) return;
    write_le_u16(wav->fp, (uint16_t)sample);
    wav->samples_written++;
}

static void wav_close(wav_writer_t *wav)
{
    if (!wav->fp) return;

    uint32_t data_bytes = wav->samples_written * 2;
    uint32_t riff_size = 36 + data_bytes;
    fseek(wav->fp, 4, SEEK_SET);
    write_le_u32(wav->fp, riff_size);
    fseek(wav->fp, 40, SEEK_SET);
    write_le_u32(wav->fp, data_bytes);
    fclose(wav->fp);
    wav->fp = NULL;
}

static bool file_exists(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) return false;
    fclose(fp);
    return true;
}

static void append_csv_summary(const app_config_t *cfg, const stats_t *stats, const wav_writer_t *wav)
{
    if (!cfg->csv_path) return;

    bool exists = file_exists(cfg->csv_path);
    FILE *fp = fopen(cfg->csv_path, "a");
    if (!fp) { fprintf(stderr, "WARNING: could not open CSV log: %s\n", cfg->csv_path); return; }

    if (!exists) {
        fprintf(fp,
                "timestamp,preset,mode,freq_hz,seconds,wav_path,sample_rate_hz,audio_rate_hz,"
                "gain_mode,manual_gain_db,squelch_enabled,squelch_db,"
                "rf_avg_dbfs,rf_min_dbfs,rf_max_dbfs,audio_rms,audio_peak,"
                "squelch_open_pct,audio_samples\n");
    }

    char timestamp[32];
    make_timestamp(timestamp, sizeof(timestamp));

    double rf_avg = stats->raw_samples > 0 ? stats->rf_sum_db / (double)stats->raw_samples : -999.0;
    double audio_rms = stats->audio_samples > 0 ? sqrt(stats->audio_sum_sq / (double)stats->audio_samples) : 0.0;
    double open_pct = stats->raw_samples > 0 ? (100.0 * (double)stats->squelch_open_raw_samples / (double)stats->raw_samples) : 0.0;

    fprintf(fp,
            "%s,%s,%s,%lld,%d,%s,%lld,%d,%s,%lld,%d,%.2f,"
            "%.2f,%.2f,%.2f,%.6f,%.6f,%.2f,%u\n",
            timestamp,
            cfg->preset_name ? cfg->preset_name : "",
            cfg->mode,
            cfg->freq_hz,
            cfg->seconds,
            cfg->wav_path,
            cfg->sample_rate_hz,
            cfg->audio_rate_hz,
            cfg->gain_mode,
            cfg->use_manual_gain ? cfg->manual_gain_db : 0,
            cfg->squelch_enabled ? 1 : 0,
            cfg->squelch_db,
            rf_avg,
            stats->rf_min_db,
            stats->rf_max_db,
            audio_rms,
            stats->audio_peak,
            open_pct,
            wav->samples_written);
    fclose(fp);
}

static int write_attr_ll(struct iio_channel *ch, const char *attr, long long value)
{
    if (!ch) { fprintf(stderr, "WARNING: missing channel for attr %s\n", attr); return -1; }
    int ret = iio_channel_attr_write_longlong(ch, attr, value);
    if (ret < 0) fprintf(stderr, "WARNING: could not set %s = %lld, ret=%d\n", attr, value, ret);
    return ret;
}

static int write_attr_str(struct iio_channel *ch, const char *attr, const char *value)
{
    if (!ch) { fprintf(stderr, "WARNING: missing channel for attr %s\n", attr); return -1; }
    int ret = iio_channel_attr_write(ch, attr, value);
    if (ret < 0) fprintf(stderr, "WARNING: could not set %s = %s, ret=%d\n", attr, value, ret);
    return ret;
}

static int16_t float_to_s16(double x)
{
    if (x > 1.0) x = 1.0;
    else if (x < -1.0) x = -1.0;
    return (int16_t)lrint(x * 32767.0);
}

static void cleanup(struct iio_buffer *rxbuf, struct iio_context *ctx, wav_writer_t *wav)
{
    if (rxbuf) iio_buffer_destroy(rxbuf);
    if (ctx) iio_context_destroy(ctx);
    if (wav) wav_close(wav);
}

static void stats_init(stats_t *stats)
{
    memset(stats, 0, sizeof(*stats));
    stats->rf_min_db = 999.0;
    stats->rf_max_db = -999.0;
}

static void stats_update_rf(stats_t *stats, double rf_db, bool squelch_open)
{
    stats->raw_samples++;
    stats->rf_sum_db += rf_db;
    if (rf_db < stats->rf_min_db) stats->rf_min_db = rf_db;
    if (rf_db > stats->rf_max_db) stats->rf_max_db = rf_db;
    if (squelch_open) stats->squelch_open_raw_samples++;
    else stats->squelch_closed_raw_samples++;
}

static void stats_update_audio(stats_t *stats, double audio)
{
    double abs_audio = fabs(audio);
    stats->audio_samples++;
    stats->audio_sum_sq += audio * audio;
    if (abs_audio > stats->audio_peak) stats->audio_peak = abs_audio;
}

static void stats_maybe_print(stats_t *stats, uint32_t audio_written, uint32_t audio_rate, uint32_t target_audio_samples)
{
    if (audio_rate == 0) return;
    uint32_t sec = audio_written / audio_rate;
    if (sec == stats->last_printed_second) return;

    stats->last_printed_second = sec;

    double rf_avg = stats->raw_samples > 0 ? stats->rf_sum_db / (double)stats->raw_samples : -999.0;
    double audio_rms = stats->audio_samples > 0 ? sqrt(stats->audio_sum_sq / (double)stats->audio_samples) : 0.0;
    double open_pct = stats->raw_samples > 0 ? (100.0 * (double)stats->squelch_open_raw_samples / (double)stats->raw_samples) : 0.0;

    printf("\r%3u sec  audio %u/%u  RF avg %.1f dBFS max %.1f  audio RMS %.4f peak %.4f  squelch %.1f%% open",
           sec, audio_written, target_audio_samples, rf_avg, stats->rf_max_db, audio_rms, stats->audio_peak, open_pct);
    fflush(stdout);
}

int main(int argc, char **argv)
{
    app_config_t cfg;
    if (!parse_args(argc, argv, &cfg)) {
        print_usage(argv[0]);
        return 1;
    }

    printf("\nPluto+ Audio Monitor\n--------------------\n");
    printf("URI:              %s\n", cfg.uri);
    printf("Mode:             %s\n", cfg.mode);
    printf("Preset:           %s\n", cfg.preset_name ? cfg.preset_name : "(none)");
    printf("Frequency:        %lld Hz\n", cfg.freq_hz);
    printf("Sample rate:      %lld Hz\n", cfg.sample_rate_hz);
    printf("RF bandwidth:     %lld Hz\n", cfg.bandwidth_hz);
    printf("Audio rate:       %d Hz\n", cfg.audio_rate_hz);
    printf("Audio low-pass:   %.1f Hz\n", cfg.audio_lowpass_hz);
    if (strcmp(cfg.mode, "wbfm") == 0) printf("De-emphasis:      %.1f us\n", cfg.deemphasis_us);
    printf("Duration:         %d seconds\n", cfg.seconds);
    printf("WAV output:       %s\n", cfg.wav_path);
    printf("CSV log:          %s\n", cfg.csv_path ? cfg.csv_path : "(none)");
    printf("Gain mode:        %s\n", cfg.gain_mode);
    if (cfg.use_manual_gain) printf("Manual gain:      %lld dB\n", cfg.manual_gain_db);
    printf("Squelch:          %s", cfg.squelch_enabled ? "enabled" : "disabled");
    if (cfg.squelch_enabled) printf(", %.1f dBFS", cfg.squelch_db);
    printf("\n\n");

    wav_writer_t wav;
    struct iio_context *ctx = NULL;
    struct iio_buffer *rxbuf = NULL;

    if (!wav_open(&wav, cfg.wav_path, (uint32_t)cfg.audio_rate_hz)) return 1;

    printf("Opening IIO context: %s\n", cfg.uri);
    ctx = iio_create_context_from_uri(cfg.uri);
    if (!ctx) { fprintf(stderr, "ERROR: Could not open IIO context: %s\n", cfg.uri); cleanup(rxbuf, ctx, &wav); return 1; }

    struct iio_device *phy = iio_context_find_device(ctx, "ad9361-phy");
    struct iio_device *rxdev = iio_context_find_device(ctx, "cf-ad9361-lpc");

    if (!phy || !rxdev) { fprintf(stderr, "ERROR: Missing ad9361-phy or cf-ad9361-lpc\n"); cleanup(rxbuf, ctx, &wav); return 1; }

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
    if (cfg.use_manual_gain) write_attr_ll(phy_rx0, "hardwaregain", cfg.manual_gain_db);
    write_attr_ll(rx_lo, "frequency", cfg.freq_hz);

    iio_channel_enable(rx0_i);
    iio_channel_enable(rx0_q);

    rxbuf = iio_device_create_buffer(rxdev, (size_t)cfg.buffer_samples, false);
    if (!rxbuf) { fprintf(stderr, "ERROR: Could not create RX buffer\n"); cleanup(rxbuf, ctx, &wav); return 1; }

    printf("Discarding initial buffers...\n");
    for (int i = 0; i < 3; i++) iio_buffer_refill(rxbuf);

    printf("Recording audio...\n");

    const uint32_t target_audio_samples = (uint32_t)cfg.seconds * (uint32_t)cfg.audio_rate_hz;
    const double input_rate = (double)cfg.sample_rate_hz;
    const double output_rate = (double)cfg.audio_rate_hz;
    const double output_step = input_rate / output_rate;
    const double lp_alpha = 1.0 - exp((-2.0 * PI_CONST * cfg.audio_lowpass_hz) / input_rate);
    const double am_dc_alpha = 1.0 - exp((-2.0 * PI_CONST * 20.0) / input_rate);
    const double deemph_alpha = (1.0 / input_rate) / ((cfg.deemphasis_us * 1e-6) + (1.0 / input_rate));

    double prev_i = 1.0;
    double prev_q = 0.0;
    bool have_prev = false;

    double filt_prev = 0.0;
    double filt_current = 0.0;
    double am_dc = 0.0;
    double deemph = 0.0;

    double input_index = 0.0;
    double next_output_index = 1.0;

    stats_t stats;
    stats_init(&stats);

    while (wav.samples_written < target_audio_samples) {
        ssize_t nbytes = iio_buffer_refill(rxbuf);
        if (nbytes < 0) {
            fprintf(stderr, "\nERROR: RX buffer refill failed: %zd\n", nbytes);
            append_csv_summary(&cfg, &stats, &wav);
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
            double mag2 = i_now * i_now + q_now * q_now;
            double mag = sqrt(mag2);
            double rf_db = 10.0 * log10(mag2 + 1e-12);
            bool squelch_open = (!cfg.squelch_enabled) || (rf_db >= cfg.squelch_db);

            stats_update_rf(&stats, rf_db, squelch_open);

            double audio = 0.0;

            if (strcmp(cfg.mode, "am") == 0) {
                am_dc = am_dc + am_dc_alpha * (mag - am_dc);
                audio = (mag - am_dc) * cfg.volume * 4.0;
            } else {
                if (!have_prev) {
                    prev_i = i_now;
                    prev_q = q_now;
                    have_prev = true;
                    continue;
                }

                double cross = prev_i * q_now - prev_q * i_now;
                double dot = prev_i * i_now + prev_q * q_now;
                double dphi = atan2(cross, dot);

                prev_i = i_now;
                prev_q = q_now;

                audio = dphi * cfg.volume;

                if (strcmp(cfg.mode, "wbfm") == 0) {
                    deemph = deemph + deemph_alpha * (audio - deemph);
                    audio = deemph;
                }
            }

            if (!squelch_open) audio = 0.0;

            filt_prev = filt_current;
            filt_current = filt_current + lp_alpha * (audio - filt_current);

            input_index += 1.0;

            while (wav.samples_written < target_audio_samples && input_index >= next_output_index) {
                double frac = 1.0 - (input_index - next_output_index);
                if (frac < 0.0) frac = 0.0;
                else if (frac > 1.0) frac = 1.0;

                double interpolated = filt_prev + frac * (filt_current - filt_prev);
                stats_update_audio(&stats, interpolated);
                wav_write_sample(&wav, float_to_s16(interpolated));

                next_output_index += output_step;
            }
        }

        stats_maybe_print(&stats, wav.samples_written, (uint32_t)cfg.audio_rate_hz, target_audio_samples);
    }

    printf("\nDone.\n");

    double rf_avg = stats.raw_samples > 0 ? stats.rf_sum_db / (double)stats.raw_samples : -999.0;
    double audio_rms = stats.audio_samples > 0 ? sqrt(stats.audio_sum_sq / (double)stats.audio_samples) : 0.0;
    double open_pct = stats.raw_samples > 0 ? (100.0 * (double)stats.squelch_open_raw_samples / (double)stats.raw_samples) : 0.0;

    printf("RF average:            %.2f dBFS\n", rf_avg);
    printf("RF minimum:            %.2f dBFS\n", stats.rf_min_db);
    printf("RF maximum:            %.2f dBFS\n", stats.rf_max_db);
    printf("Audio RMS:             %.6f\n", audio_rms);
    printf("Audio peak:            %.6f\n", stats.audio_peak);
    printf("Squelch open:          %.2f %%\n", open_pct);
    printf("Raw IQ samples:        %llu\n", (unsigned long long)stats.raw_samples);
    printf("Audio samples written: %u\n", wav.samples_written);
    printf("WAV file:              %s\n", cfg.wav_path);

    append_csv_summary(&cfg, &stats, &wav);

    if (cfg.csv_path) printf("CSV log:               %s\n", cfg.csv_path);

    cleanup(rxbuf, ctx, &wav);
    return 0;
}
