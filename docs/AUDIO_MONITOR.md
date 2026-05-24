# Pluto+ Audio Monitor

`pluto_audio_monitor.exe` records demodulated audio from the Pluto+ SDR into a mono 16-bit PCM WAV file.

## Supported modes

```text
nfm  Narrow FM demodulation, useful for NOAA and 2m FM voice
am   AM envelope demodulation, useful for VHF airband
```

## Output folder convention

Use the `sessions/` folder for generated WAV and CSV files:

```bash
mkdir -p sessions
```

## NOAA NFM test

```bash
./build/native/pluto_audio_monitor.exe \
  --preset noaa7 \
  --seconds 10 \
  --squelch-db -65 \
  --wav sessions/noaa_v2_test.wav \
  --csv sessions/audio_log.csv
```

## Airband AM test

```bash
./build/native/pluto_audio_monitor.exe \
  --mode am \
  --preset airband-125 \
  --seconds 20 \
  --squelch-db -65 \
  --wav sessions/airband_am_test.wav \
  --csv sessions/audio_log.csv
```

## Airband presets

```text
airband-118   118.000 MHz
airband-120   120.000 MHz
airband-1228  122.800 MHz
airband-125   125.000 MHz
airband-1275  127.500 MHz
airband-130   130.000 MHz
```

## Useful AM options

If the airband channel is quiet or intermittent, use a longer recording:

```bash
./build/native/pluto_audio_monitor.exe \
  --mode am \
  --preset airband-125 \
  --seconds 120 \
  --squelch-db -70 \
  --wav sessions/airband_long.wav \
  --csv sessions/audio_log.csv
```

If squelch is closing too aggressively:

```text
--squelch-db -75
```

If you want to record everything:

```text
--squelch-off
```

If audio is too low:

```text
--volume 5
```

If audio clips:

```text
--volume 1.5
```
