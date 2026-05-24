# Pluto+ Audio Monitor

`pluto_audio_monitor.exe` records demodulated audio from the Pluto+ SDR into a mono 16-bit PCM WAV file.

## Supported modes

```text
nfm   Narrow FM demodulation, useful for NOAA and 2m FM voice
am    AM envelope demodulation, useful for VHF airband
wbfm  Wide FM demodulation, useful for FM broadcast band tests
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
  --wav sessions/noaa.wav \
  --csv sessions/audio_log.csv
```

## Airband AM test

```bash
./build/native/pluto_audio_monitor.exe \
  --mode am \
  --preset airband-125 \
  --seconds 30 \
  --squelch-db -65 \
  --wav sessions/airband_am.wav \
  --csv sessions/audio_log.csv
```

## Broadcast FM WBFM test

```bash
./build/native/pluto_audio_monitor.exe \
  --preset fm-100 \
  --seconds 30 \
  --squelch-off \
  --wav sessions/fm100.wav \
  --csv sessions/audio_log.csv
```

## FM broadcast presets

```text
fm-88   88.000 MHz
fm-90   90.000 MHz
fm-94   94.000 MHz
fm-98   98.000 MHz
fm-100  100.000 MHz
fm-102  102.000 MHz
fm-104  104.000 MHz
fm-106  106.000 MHz
```

## WBFM notes

This is mono WBFM audio only. It does not decode stereo multiplex or RDS.

WBFM defaults:

```text
sample rate:      2400000
RF bandwidth:     1800000
audio low-pass:   15000 Hz
de-emphasis:      75 us
```

If audio is too low:

```text
--volume 1.5
```

If audio clips:

```text
--volume 0.4
```
