# Pluto+ Audio Monitor v2

`pluto_audio_monitor.exe` records narrow-FM audio from the Pluto+ SDR into a mono 16-bit PCM WAV file.

## Important path note

The audio monitor writes `--wav` and `--csv` exactly where you tell it.

If you run this from the repo root:

```bash
./build/native/pluto_audio_monitor.exe --preset noaa7 --wav noaa.wav --csv audio_log.csv
```

then the files are written to the repo root:

```text
C:\msys64\home\jim\sdrdev\pluto_native_test\
```

Recommended project convention:

```bash
mkdir -p sessions
./build/native/pluto_audio_monitor.exe \
  --preset noaa7 \
  --seconds 10 \
  --squelch-db -65 \
  --wav sessions/noaa_v2_test.wav \
  --csv sessions/audio_log.csv
```

That writes:

```text
sessions\noaa_v2_test.wav
sessions\audio_log.csv
```

The `launchers\run_noaa_audio.cmd` launcher already changes into the `sessions\` folder before running the tool, so its output goes to `sessions\` automatically.

## New in v2

- RF squelch with `--squelch-db`
- `--squelch-off`
- Per-second console stats
- Final RF/audio/squelch summary
- Optional summary CSV append log with `--csv`
- NOAA presets
- Automatic timestamped WAV filename if `--wav` is omitted

## Build

```bash
./tools/build_native_ucrt64.sh
```

## NOAA test

```bash
mkdir -p sessions

./build/native/pluto_audio_monitor.exe \
  --preset noaa7 \
  --seconds 10 \
  --squelch-db -65 \
  --wav sessions/noaa_v2_test.wav \
  --csv sessions/audio_log.csv
```

## NOAA presets

```text
noaa1        162.400 MHz
noaa2        162.425 MHz
noaa3        162.450 MHz
noaa4        162.475 MHz
noaa5        162.500 MHz
noaa6        162.525 MHz
noaa7        162.550 MHz
noaa-162550  162.550 MHz
```

## Disable squelch

```bash
./build/native/pluto_audio_monitor.exe \
  --preset noaa7 \
  --seconds 10 \
  --squelch-off \
  --wav sessions/noaa_no_squelch.wav
```

## CSV log

```bash
./build/native/pluto_audio_monitor.exe \
  --preset noaa7 \
  --seconds 10 \
  --wav sessions/noaa.wav \
  --csv sessions/audio_log.csv
```

## Open output folder

```bash
explorer.exe sessions
```
