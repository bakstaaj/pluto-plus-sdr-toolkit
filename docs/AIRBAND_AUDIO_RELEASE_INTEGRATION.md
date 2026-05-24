# Airband Audio Release Integration

The Windows release package now includes both audio launchers:

```text
launchers\run_noaa_audio.cmd
launchers\run_airband_audio.cmd
```

Both launchers write WAV and CSV output to:

```text
sessions\
```

## Build release

```bash
./tools/package_windows_release.sh v0.9-airband-audio
```

## Verify release contents

```bash
cat releases/pluto-plus-sdr-toolkit-v0.9-airband-audio/MANIFEST.txt

ls -la releases/pluto-plus-sdr-toolkit-v0.9-airband-audio/bin/native/pluto_audio_monitor.exe
ls -la releases/pluto-plus-sdr-toolkit-v0.9-airband-audio/launchers/run_noaa_audio.cmd
ls -la releases/pluto-plus-sdr-toolkit-v0.9-airband-audio/launchers/run_airband_audio.cmd
```

## Test NOAA audio

```bash
cmd.exe /c releases\\pluto-plus-sdr-toolkit-v0.9-airband-audio\\launchers\\run_noaa_audio.cmd
```

Expected outputs:

```text
sessions\noaa.wav
sessions\audio_log.csv
```

## Test airband AM audio

```bash
cmd.exe /c releases\\pluto-plus-sdr-toolkit-v0.9-airband-audio\\launchers\\run_airband_audio.cmd
```

Expected outputs:

```text
sessions\airband_am.wav
sessions\audio_log.csv
```

Airband transmissions are intermittent. Try:

```text
run_airband_audio.cmd --seconds 180 --squelch-db -70
```
