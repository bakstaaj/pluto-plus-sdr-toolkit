# Audio Report Generator

`pluto_audio_report.exe` reads `audio_log.csv` from the audio monitor and creates an HTML report.

The report includes:

```text
recording count
mode counts
RF summary
audio peak summary
table of recordings
embedded WAV players
```

## Build

```bash
./tools/build_native_ucrt64.sh
```

## Generate report

```bash
./build/native/pluto_audio_report.exe \
  --in sessions/audio_log.csv \
  --out sessions/audio_report.html
```

## Open report

```bash
explorer.exe sessions/audio_report.html
```

## Launcher

```bash
cmd.exe /c launchers\\make_audio_report.cmd
```

## Notes

The audio report uses the WAV paths stored in `audio_log.csv`.

If the audio monitor was run from the `sessions\` folder, the WAV paths will normally be simple filenames such as:

```text
noaa.wav
airband_am.wav
fm100.wav
```

That is ideal because the HTML report is also written to `sessions\`.
