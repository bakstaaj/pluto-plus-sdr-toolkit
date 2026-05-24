# Audio Report Release Integration

The release package now includes the audio report generator and launcher:

```text
bin\native\pluto_audio_report.exe
launchers\make_audio_report.cmd
```

## Build release

```bash
rm -rf releases/pluto-plus-sdr-toolkit-v1.3-audio-report
rm -f releases/pluto-plus-sdr-toolkit-v1.3-audio-report.zip

./tools/package_windows_release.sh v1.3-audio-report
```

## Verify

```bash
cat releases/pluto-plus-sdr-toolkit-v1.3-audio-report/MANIFEST.txt

ls -la releases/pluto-plus-sdr-toolkit-v1.3-audio-report/bin/native/pluto_audio_report.exe
ls -la releases/pluto-plus-sdr-toolkit-v1.3-audio-report/launchers/make_audio_report.cmd
```

## Test workflow

Run audio menu first:

```bash
cmd.exe /c releases\\pluto-plus-sdr-toolkit-v1.3-audio-report\\launchers\\run_audio_menu.cmd
```

Record at least one WAV.

Then generate report:

```bash
cmd.exe /c releases\\pluto-plus-sdr-toolkit-v1.3-audio-report\\launchers\\make_audio_report.cmd
```

Expected outputs:

```text
sessions\audio_log.csv
sessions\audio_report.html
```

The HTML report contains embedded WAV players for recordings listed in `audio_log.csv`.
