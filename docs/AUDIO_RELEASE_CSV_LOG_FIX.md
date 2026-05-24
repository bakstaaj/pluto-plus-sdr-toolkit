# Audio Release CSV Log Fix

The release `run_noaa_audio.cmd` launcher must pass:

```text
--csv audio_log.csv
```

to `pluto_audio_monitor.exe`.

Without that option, the WAV file is created, but `sessions\audio_log.csv` is not created.

## Repair an existing release

```bash
./tools/create_release_launchers.sh releases/pluto-plus-sdr-toolkit-v0.8-audio-v2
```

Then rerun:

```bash
cmd.exe /c releases\\pluto-plus-sdr-toolkit-v0.8-audio-v2\\launchers\\run_noaa_audio.cmd
```

Verify:

```bash
ls -lh releases/pluto-plus-sdr-toolkit-v0.8-audio-v2/sessions/noaa.wav
ls -lh releases/pluto-plus-sdr-toolkit-v0.8-audio-v2/sessions/audio_log.csv
```

## Confirm the launcher includes CSV logging

```bash
grep -n -- "--csv" releases/pluto-plus-sdr-toolkit-v0.8-audio-v2/launchers/run_noaa_audio.cmd
```
