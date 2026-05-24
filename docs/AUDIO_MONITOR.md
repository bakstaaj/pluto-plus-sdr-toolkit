# Pluto+ Audio Monitor

`pluto_audio_monitor.exe` is the first audio/demod milestone.

Current supported mode:

```text
nfm - narrow FM demodulation to mono 16-bit PCM WAV
```

## Good first target: NOAA

NOAA weather radio is the best first validation target because it is usually continuous.

Common NOAA frequencies:

```text
162400000
162425000
162450000
162475000
162500000
162525000
162550000
```

## Build

From MSYS2 UCRT64:

```bash
./tools/build_native_ucrt64.sh
```

## First test

```bash
./build/native/pluto_audio_monitor.exe \
  --mode nfm \
  --freq 162550000 \
  --rate 960000 \
  --audio-rate 48000 \
  --seconds 30 \
  --wav noaa.wav
```

The default SDR sample rate is `960000`, which divides cleanly to `48000` audio.

## Manual gain test

```bash
./build/native/pluto_audio_monitor.exe \
  --mode nfm \
  --freq 162550000 \
  --rate 960000 \
  --audio-rate 48000 \
  --gain-mode manual \
  --gain-db 40 \
  --seconds 30 \
  --wav noaa_manual.wav
```

## 2m NFM quick test

```bash
./build/native/pluto_audio_monitor.exe \
  --mode nfm \
  --freq 146520000 \
  --rate 960000 \
  --audio-rate 48000 \
  --seconds 30 \
  --wav 2m_call.wav
```

## Tuning tips

If audio is too quiet:

```bash
--volume 5
```

If audio clips or sounds harsh:

```bash
--volume 1.5
```

If you hear hiss but no voice, try the other NOAA channels.

If the signal is weak, try manual gain:

```bash
--gain-mode manual --gain-db 40
```

## Next planned modes

Future milestones:

```text
am    - airband AM audio
wbfm  - FM broadcast audio
nfm   - improved squelch and live playback
```
