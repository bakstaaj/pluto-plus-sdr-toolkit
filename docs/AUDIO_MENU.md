# Audio Menu Launcher

The audio menu launcher provides a simple text menu for the three audio modes:

```text
NFM   NOAA / 2m voice
AM    Airband voice
WBFM  FM broadcast mono audio
```

## Repo launcher

```bash
cmd.exe /c launchers\\run_audio_menu.cmd
```

## Release launcher

```bash
cmd.exe /c releases\\pluto-plus-sdr-toolkit-v1.1-audio-menu\\launchers\\run_audio_menu.cmd
```

## Outputs

All WAV and CSV files are written to:

```text
sessions\
```

The menu includes:

```text
NOAA NFM default
NOAA preset choice
Airband AM default
Airband AM long capture
Broadcast FM WBFM default
FM preset choice
Custom mode/frequency
Open sessions folder
```

## Notes

The menu uses `pluto_audio_monitor.exe`. Build native tools first:

```bash
./tools/build_native_ucrt64.sh
```

Then run:

```bash
cmd.exe /c launchers\\run_audio_menu.cmd
```
