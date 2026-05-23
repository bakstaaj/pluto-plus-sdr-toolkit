#include <iio.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    const char *uri = "ip:192.168.2.1";

    if (argc >= 2) {
        uri = argv[1];
    }

    unsigned int major = 0;
    unsigned int minor = 0;
    char git_tag[16] = {0};

    iio_library_get_version(&major, &minor, git_tag);

    printf("libiio version: %u.%u %s\n", major, minor, git_tag);
    printf("Opening IIO context: %s\n\n", uri);

    struct iio_context *ctx = iio_create_context_from_uri(uri);
    if (!ctx) {
        fprintf(stderr, "ERROR: Could not open IIO context: %s\n", uri);
        fprintf(stderr, "Try this first:\n");
        fprintf(stderr, "  iio_info -u %s\n", uri);
        return 1;
    }

    const char *ctx_name = iio_context_get_name(ctx);
    const char *ctx_desc = iio_context_get_description(ctx);

    printf("Context name:        %s\n", ctx_name ? ctx_name : "(none)");
    printf("Context description: %s\n\n", ctx_desc ? ctx_desc : "(none)");

    unsigned int dev_count = iio_context_get_devices_count(ctx);
    printf("Devices found: %u\n\n", dev_count);

    for (unsigned int d = 0; d < dev_count; d++) {
        struct iio_device *dev = iio_context_get_device(ctx, d);

        const char *dev_id = iio_device_get_id(dev);
        const char *dev_name = iio_device_get_name(dev);

        printf("[%u] id=%s name=%s channels=%u\n",
               d,
               dev_id ? dev_id : "(none)",
               dev_name ? dev_name : "(none)",
               iio_device_get_channels_count(dev));

        for (unsigned int c = 0; c < iio_device_get_channels_count(dev); c++) {
            struct iio_channel *ch = iio_device_get_channel(dev, c);

            const char *ch_id = iio_channel_get_id(ch);
            const char *direction = iio_channel_is_output(ch) ? "output" : "input";
            const char *scan = iio_channel_is_scan_element(ch) ? "scan" : "control";

            printf("    channel %-16s %-6s %s\n",
                   ch_id ? ch_id : "(none)",
                   direction,
                   scan);
        }

        printf("\n");
    }

    iio_context_destroy(ctx);
    return 0;
}
