#include <iio.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <math.h>

static int write_attr_ll(struct iio_channel *ch, const char *attr, long long value)
{
    if (!ch) {
        printf("WARNING: missing channel for attr %s\n", attr);
        return -1;
    }

    int ret = iio_channel_attr_write_longlong(ch, attr, value);
    if (ret < 0) {
        printf("WARNING: could not set %s = %lld, ret=%d\n", attr, value, ret);
    }

    return ret;
}

static int write_attr_str(struct iio_channel *ch, const char *attr, const char *value)
{
    if (!ch) {
        printf("WARNING: missing channel for attr %s\n", attr);
        return -1;
    }

    int ret = iio_channel_attr_write(ch, attr, value);
    if (ret < 0) {
        printf("WARNING: could not set %s = %s, ret=%d\n", attr, value, ret);
    }

    return ret;
}

int main(int argc, char **argv)
{
    const char *uri = "ip:192.168.2.1";

    long long rx_lo_hz = 100000000LL;
    long long sample_rate_hz = 1000000LL;
    long long rf_bandwidth_hz = 1000000LL;
    size_t buffer_samples = 16384;

    if (argc >= 2) {
        uri = argv[1];
    }

    printf("Opening Pluto+ context: %s\n", uri);

    struct iio_context *ctx = iio_create_context_from_uri(uri);
    if (!ctx) {
        fprintf(stderr, "ERROR: Could not open IIO context.\n");
        fprintf(stderr, "Try: iio_info -u %s\n", uri);
        return 1;
    }

    struct iio_device *phy = iio_context_find_device(ctx, "ad9361-phy");
    struct iio_device *rxdev = iio_context_find_device(ctx, "cf-ad9361-lpc");

    if (!phy) {
        fprintf(stderr, "ERROR: Could not find ad9361-phy.\n");
        iio_context_destroy(ctx);
        return 1;
    }

    if (!rxdev) {
        fprintf(stderr, "ERROR: Could not find cf-ad9361-lpc RX buffer device.\n");
        iio_context_destroy(ctx);
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
        fprintf(stderr, "ERROR: Could not find RX0 I/Q scan channels voltage0 and voltage1.\n");
        iio_context_destroy(ctx);
        return 1;
    }

    /*
       Control-side PHY channels.
       voltage0 input controls RX0 settings.
       altvoltage0 output is normally RX LO.
    */
    struct iio_channel *phy_rx0 = iio_device_find_channel(phy, "voltage0", false);
    struct iio_channel *rx_lo = iio_device_find_channel(phy, "altvoltage0", true);

    if (!phy_rx0) {
        fprintf(stderr, "ERROR: Could not find PHY RX0 control channel voltage0.\n");
        iio_context_destroy(ctx);
        return 1;
    }

    if (!rx_lo) {
        fprintf(stderr, "ERROR: Could not find RX LO channel altvoltage0.\n");
        iio_context_destroy(ctx);
        return 1;
    }

    printf("Configuring RX:\n");
    printf("  LO frequency:   %lld Hz\n", rx_lo_hz);
    printf("  Sample rate:    %lld Hz\n", sample_rate_hz);
    printf("  RF bandwidth:   %lld Hz\n", rf_bandwidth_hz);
    printf("  Buffer samples: %zu\n", buffer_samples);

    write_attr_ll(phy_rx0, "sampling_frequency", sample_rate_hz);
    write_attr_ll(phy_rx0, "rf_bandwidth", rf_bandwidth_hz);
    write_attr_str(phy_rx0, "gain_control_mode", "slow_attack");
    write_attr_ll(rx_lo, "frequency", rx_lo_hz);

    iio_channel_enable(rx0_i);
    iio_channel_enable(rx0_q);

    struct iio_buffer *rxbuf = iio_device_create_buffer(rxdev, buffer_samples, false);
    if (!rxbuf) {
        fprintf(stderr, "ERROR: Could not create RX buffer.\n");
        fprintf(stderr, "Possible causes:\n");
        fprintf(stderr, "  - another program is using the Pluto+\n");
        fprintf(stderr, "  - RX channels were not enabled\n");
        fprintf(stderr, "  - wrong RX device name\n");
        iio_context_destroy(ctx);
        return 1;
    }

    printf("\nRefilling RX buffer...\n");

    ssize_t nbytes = iio_buffer_refill(rxbuf);
    if (nbytes < 0) {
        fprintf(stderr, "ERROR: RX buffer refill failed: %zd\n", nbytes);
        iio_buffer_destroy(rxbuf);
        iio_context_destroy(ctx);
        return 1;
    }

    printf("Received %zd bytes\n", nbytes);

    char *p_i = (char *)iio_buffer_first(rxbuf, rx0_i);
    char *p_q = (char *)iio_buffer_first(rxbuf, rx0_q);
    char *p_end = (char *)iio_buffer_end(rxbuf);
    ptrdiff_t p_inc = iio_buffer_step(rxbuf);

    long long count = 0;
    double sum_mag = 0.0;
    double max_mag = 0.0;

    FILE *f = fopen("capture_iq_s16le.raw", "wb");
    if (!f) {
        fprintf(stderr, "ERROR: Could not open capture_iq_s16le.raw for writing.\n");
        iio_buffer_destroy(rxbuf);
        iio_context_destroy(ctx);
        return 1;
    }

    for (; p_i < p_end && p_q < p_end; p_i += p_inc, p_q += p_inc) {
        int16_t i_sample = *(int16_t *)p_i;
        int16_t q_sample = *(int16_t *)p_q;

        fwrite(&i_sample, sizeof(int16_t), 1, f);
        fwrite(&q_sample, sizeof(int16_t), 1, f);

        double mag = sqrt((double)i_sample * (double)i_sample +
                          (double)q_sample * (double)q_sample);

        sum_mag += mag;

        if (mag > max_mag) {
            max_mag = mag;
        }

        count++;
    }

    fclose(f);

    printf("IQ samples written: %lld complex samples\n", count);
    printf("Mean magnitude:     %.2f\n", count > 0 ? sum_mag / count : 0.0);
    printf("Peak magnitude:     %.2f\n", max_mag);
    printf("Output file:        capture_iq_s16le.raw\n");

    iio_buffer_destroy(rxbuf);
    iio_context_destroy(ctx);

    return 0;
}
