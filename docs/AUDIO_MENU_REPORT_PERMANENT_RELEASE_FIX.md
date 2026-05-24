# Permanent Audio Menu Report Workflow

`tools/create_release_launchers.sh` now permanently generates the report-aware audio menu.

## New permanent release behavior

Generated release menu:

```text
launchers\run_audio_menu.cmd
```

now includes:

```text
8.  Generate / open audio HTML report
9.  Open sessions folder
10. Exit
```

After a successful recording, it prompts:

```text
Generate/update audio_report.html now? [Y/n]:
```

## Build validation release

```bash
rm -rf releases/pluto-plus-sdr-toolkit-v1.4-audio-report-menu
rm -f releases/pluto-plus-sdr-toolkit-v1.4-audio-report-menu.zip

./tools/package_windows_release.sh v1.4-audio-report-menu
```

## Verify generated release launcher

```bash
grep -n '"%AUDIO_EXE%" !ARGS!' \
  releases/pluto-plus-sdr-toolkit-v1.4-audio-report-menu/launchers/run_audio_menu.cmd

grep -n "Generate/update audio_report.html now" \
  releases/pluto-plus-sdr-toolkit-v1.4-audio-report-menu/launchers/run_audio_menu.cmd

grep -n "pluto_audio_report.exe" \
  releases/pluto-plus-sdr-toolkit-v1.4-audio-report-menu/launchers/run_audio_menu.cmd
```

All three commands should return matches.

## Test release workflow

```bash
cmd.exe /c releases\\pluto-plus-sdr-toolkit-v1.4-audio-report-menu\\launchers\\run_audio_menu.cmd
```

Test:

```text
1. Record NOAA NFM
2. Accept report generation prompt
3. Confirm sessions\audio_report.html opens
```

## Commit

```bash
git add tools/create_release_launchers.sh docs/AUDIO_MENU_REPORT_PERMANENT_RELEASE_FIX.md
git commit -m "Bake audio report workflow into release menu"
git push
```
