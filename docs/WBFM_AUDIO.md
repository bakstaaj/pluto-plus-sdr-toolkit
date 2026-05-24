# Broadcast FM WBFM Audio

`pluto_audio_monitor.exe` now supports `--mode wbfm`.

This is useful for quickly validating the Pluto+ 70 MHz+ receive range with a strong FM broadcast station.

## Quick test

```bash
mkdir -p sessions

./build/native/pluto_audio_monitor.exe \
  --preset fm-100 \
  --seconds 30 \
  --squelch-off \
  --wav sessions/fm100.wav \
  --csv sessions/audio_log.csv
```

Then open:

```text
sessions\fm100.wav
```

## Launcher

```bash
cmd.exe /c launchers\\run_fm_audio.cmd
```

## Pick a different FM frequency

Use one of the built-in presets:

```bash
./build/native/pluto_audio_monitor.exe --preset fm-94 --seconds 30 --squelch-off --wav sessions/fm94.wav
```

Or tune directly:

```bash
./build/native/pluto_audio_monitor.exe \
  --mode wbfm \
  --freq 98700000 \
  --rate 2400000 \
  --bw 1800000 \
  --audio-lowpass-hz 15000 \
  --seconds 30 \
  --squelch-off \
  --wav sessions/fm_98_7.wav
```

## Notes

- This is mono WBFM only.
- It does not decode stereo or RDS.
- Strong local FM broadcast stations are the easiest validation target.
