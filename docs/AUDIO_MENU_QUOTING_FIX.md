# Audio Menu Quoting Fix

## Problem

The original audio menu stored the full command line in one variable and then executed that variable.

In some cases Windows `cmd.exe` treated the entire line as one executable name:

```text
'C:\...\pluto_audio_monitor.exe --mode am --freq ... --csv audio_log.csv' is not recognized
```

## Fix

The fixed launcher stores only the arguments:

```text
ARGS=--mode am --freq ...
```

Then runs the executable directly:

```text
"%AUDIO_EXE%" !ARGS!
```

This is safer and avoids the whole-command-as-one-executable problem.

## Repair existing release

```bash
./tools/repair_audio_menu_launcher.sh releases/pluto-plus-sdr-toolkit-v1.1-audio-menu
```

Then test:

```bash
cmd.exe /c releases\\pluto-plus-sdr-toolkit-v1.1-audio-menu\\launchers\\run_audio_menu.cmd
```

## Test repo launcher

```bash
cmd.exe /c launchers\\run_audio_menu.cmd
```
