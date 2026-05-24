# Permanent Audio Menu Release Fix

`tools/create_release_launchers.sh` now generates the fixed `run_audio_menu.cmd` for every future release.

## What changed

The release audio menu no longer executes a full command string variable.

Old unsafe style:

```text
set "CMD=C:\...\pluto_audio_monitor.exe --mode ..."
!CMD!
```

New safe style:

```text
set "ARGS=--mode ..."
"%AUDIO_EXE%" !ARGS!
```

## Build validation release

```bash
rm -rf releases/pluto-plus-sdr-toolkit-v1.2-audio-menu-fixed
rm -f releases/pluto-plus-sdr-toolkit-v1.2-audio-menu-fixed.zip

./tools/package_windows_release.sh v1.2-audio-menu-fixed
```

## Verify launcher contains fixed execution

```bash
grep -n '"%AUDIO_EXE%" !ARGS!' \
  releases/pluto-plus-sdr-toolkit-v1.2-audio-menu-fixed/launchers/run_audio_menu.cmd

grep -n '^set "CMD=' \
  releases/pluto-plus-sdr-toolkit-v1.2-audio-menu-fixed/launchers/run_audio_menu.cmd
```

The first command should show a match. The second command should show no output.

## Test release menu

```bash
cmd.exe /c releases\\pluto-plus-sdr-toolkit-v1.2-audio-menu-fixed\\launchers\\run_audio_menu.cmd
```

Test at least:

```text
1. NOAA NFM default
5. Broadcast FM default
7. Custom frequency/mode
```
