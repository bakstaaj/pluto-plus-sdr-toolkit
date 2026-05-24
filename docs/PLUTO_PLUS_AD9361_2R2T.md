# Pluto+ AD9361 / 2R2T Configuration

This project standardizes on the following Pluto+ runtime identity:

```text
Firmware:    ADI PlutoSDR v0.39
attr_name:   compatible
attr_val:    ad9361
compatible:  ad9361
mode:        2r2t
```

This has been validated to expose:

```text
70 MHz low-frequency range
2 RX channels
2 TX channels
```

## Configure on Pluto+

Run this on the Pluto+:

```sh
fw_setenv attr_name compatible
fw_setenv attr_val ad9361
fw_setenv compatible ad9361
fw_setenv mode 2r2t
reboot
```

Or use the guarded project helper:

```sh
/mnt/jffs2/pluto_ham_scan/tools/pluto_configure_ad9361_2r2t.sh --yes
reboot
```

To configure and reboot in one command:

```sh
/mnt/jffs2/pluto_ham_scan/tools/pluto_configure_ad9361_2r2t.sh --yes --reboot
```

## Verify on Pluto+

```sh
/mnt/jffs2/pluto_ham_scan/tools/pluto_verify_ad9361_2r2t.sh
```

Expected U-Boot environment:

```text
attr_name=compatible
attr_val=ad9361
compatible=ad9361
mode=2r2t
```

Expected IIO model:

```text
ad9361-phy,model: ad9361
```

Expected RX buffer channels:

```text
cf-ad9361-lpc:
  voltage0 input index 0
  voltage1 input index 1
  voltage2 input index 2
  voltage3 input index 3
```

That maps as:

```text
RX1 = voltage0 I + voltage1 Q
RX2 = voltage2 I + voltage3 Q
```

Expected TX buffer channels:

```text
cf-ad9361-dds-core-lpc:
  voltage0 output index 0
  voltage1 output index 1
  voltage2 output index 2
  voltage3 output index 3
```

## Deploy tools from MSYS2

```bash
cd ~/sdrdev/pluto_native_test

./tools/deploy_pluto_2r2t_tools_msys2.sh
```

The deploy script uses:

```bash
scp -O
```

for Pluto+ compatibility.

## Test dual RX low-frequency behavior

From the host:

```bash
./build/native/pluto_dual_rx_probe.exe \
  --uri ip:192.168.2.1 \
  --freq 146520000 \
  --rate 960000 \
  --bw 1000000 \
  --rx-mode dual \
  --seconds 2 \
  --csv dual_rx_probe_host.csv
```

Good result:

```text
RX1 channels: voltage0/voltage1 OK
RX2 channels: voltage2/voltage3 OK
Effective RX mode: dual
Dual available: yes
```

There should be no `could not set frequency ... ret=-22` warning.

## Notes

This setting is persistent in the Pluto U-Boot environment. If a firmware change or recovery operation resets the environment, rerun the configure step.
