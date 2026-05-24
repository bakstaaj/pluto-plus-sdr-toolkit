# Audio Release Integration

The release package now includes the NOAA NFM audio monitor when `native/src/pluto_audio_monitor.c` is present and built.

## Release command

```bash
./tools/package_windows_release.sh v0.7-audio
```

## Verify audio files in the release

```bash
ls -la releases/pluto-plus-sdr-toolkit-v0.7-audio/bin/native/pluto_audio_monitor.exe
ls -la releases/pluto-plus-sdr-toolkit-v0.7-audio/launchers/run_noaa_audio.cmd
```

## Test from the release folder

From Windows Explorer:

```text
releases\pluto-plus-sdr-toolkit-v0.7-audio\launchers\run_noaa_audio.cmd
```

The launcher records:

```text
sessions\noaa.wav
```

## Command-line release test

```bash
cmd.exe /c releases\\pluto-plus-sdr-toolkit-v0.7-audio\\launchers\\run_noaa_audio.cmd
```

## Direct native test

```bash
./releases/pluto-plus-sdr-toolkit-v0.7-audio/bin/native/pluto_audio_monitor.exe \
  --mode nfm \
  --freq 162550000 \
  --rate 960000 \
  --audio-rate 48000 \
  --seconds 10 \
  --wav releases/pluto-plus-sdr-toolkit-v0.7-audio/sessions/noaa_test.wav
```
