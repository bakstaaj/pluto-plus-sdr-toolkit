# WBFM Audio Release Integration

The Windows release package now includes all three audio launchers:

```text
launchers\run_noaa_audio.cmd
launchers\run_airband_audio.cmd
launchers\run_fm_audio.cmd
```

All three launchers write WAV and CSV output to:

```text
sessions\
```

## Build release

```bash
./tools/package_windows_release.sh v1.0-audio-modes
```

## Verify release contents

```bash
cat releases/pluto-plus-sdr-toolkit-v1.0-audio-modes/MANIFEST.txt

ls -la releases/pluto-plus-sdr-toolkit-v1.0-audio-modes/bin/native/pluto_audio_monitor.exe
ls -la releases/pluto-plus-sdr-toolkit-v1.0-audio-modes/launchers/run_noaa_audio.cmd
ls -la releases/pluto-plus-sdr-toolkit-v1.0-audio-modes/launchers/run_airband_audio.cmd
ls -la releases/pluto-plus-sdr-toolkit-v1.0-audio-modes/launchers/run_fm_audio.cmd
```

## Test NOAA NFM audio

```bash
cmd.exe /c releases\\pluto-plus-sdr-toolkit-v1.0-audio-modes\\launchers\\run_noaa_audio.cmd
```

Expected outputs:

```text
sessions\noaa.wav
sessions\audio_log.csv
```

## Test airband AM audio

```bash
cmd.exe /c releases\\pluto-plus-sdr-toolkit-v1.0-audio-modes\\launchers\\run_airband_audio.cmd
```

Expected outputs:

```text
sessions\airband_am.wav
sessions\audio_log.csv
```

## Test broadcast FM WBFM audio

```bash
cmd.exe /c releases\\pluto-plus-sdr-toolkit-v1.0-audio-modes\\launchers\\run_fm_audio.cmd
```

Expected outputs:

```text
sessions\fm100.wav
sessions\audio_log.csv
```

To use a different FM station:

```text
run_fm_audio.cmd --preset fm-94
run_fm_audio.cmd --mode wbfm --freq 98700000
```
