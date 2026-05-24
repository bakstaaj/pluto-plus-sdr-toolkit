# Audio Menu Report Workflow

The audio menu now includes audio report generation.

## New menu behavior

The menu now has:

```text
8. Generate / open audio HTML report
9. Open sessions folder
10. Exit
```

After a successful audio recording, the menu prompts:

```text
Generate/update audio_report.html now? [Y/n]:
```

Press Enter or `Y` to generate the report. Press `n` to skip.

## Repo test

```bash
cmd.exe /c launchers\\run_audio_menu.cmd
```

Test:

```text
1. Record NOAA audio
Accept report generation prompt
Confirm sessions\audio_report.html opens
```

## Existing release update

```bash
./tools/write_release_audio_menu.sh releases/pluto-plus-sdr-toolkit-v1.3-audio-report
```

Then test:

```bash
cmd.exe /c releases\\pluto-plus-sdr-toolkit-v1.3-audio-report\\launchers\\run_audio_menu.cmd
```

## Requirements

The report option requires:

```text
pluto_audio_report.exe
audio_log.csv
```

The menu handles missing files with clear error messages.
