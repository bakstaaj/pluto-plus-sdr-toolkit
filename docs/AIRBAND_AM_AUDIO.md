# Airband AM Audio

The audio monitor now supports AM demodulation for VHF aviation airband tests.

## Build

```bash
./tools/build_native_ucrt64.sh
```

## Quick test

```bash
mkdir -p sessions

./build/native/pluto_audio_monitor.exe \
  --mode am \
  --preset airband-125 \
  --seconds 30 \
  --squelch-db -65 \
  --wav sessions/airband_am.wav \
  --csv sessions/audio_log.csv
```

## Launcher

```bash
cmd.exe /c launchers\\run_airband_audio.cmd
```

## Notes

Airband voice traffic is intermittent. A 30 second recording may be silent if nobody is transmitting. Try longer captures during active airport periods:

```bash
./build/native/pluto_audio_monitor.exe \
  --mode am \
  --preset airband-125 \
  --seconds 180 \
  --squelch-db -70 \
  --wav sessions/airband_3min.wav \
  --csv sessions/audio_log.csv
```

The AM implementation is simple envelope demodulation:

```text
audio = magnitude(IQ) - low-pass DC estimate
```

Then the audio is low-pass filtered and written as 48 kHz mono 16-bit PCM WAV.
