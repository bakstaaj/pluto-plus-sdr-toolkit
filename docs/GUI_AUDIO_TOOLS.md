# GUI Audio Tools

Adds an **Audio Tools** strip to the top of the Pluto session GUI.

Buttons added:

```text
Audio Menu
Audio Report
Sessions Folder
```

## Install

From MSYS2 UCRT64:

```bash
cd ~/sdrdev/pluto_native_test

cp /c/Users/jim/Downloads/PlutoGuiAudioTools/tools/install_gui_audio_tools.sh tools/
chmod +x tools/install_gui_audio_tools.sh

./tools/install_gui_audio_tools.sh
```

## Build test

```bash
cd gui/PlutoGuiStarter_v3_RepoLayout
dotnet build
dotnet run
```

## Release test

```bash
cd ~/sdrdev/pluto_native_test

rm -rf releases/pluto-plus-sdr-toolkit-v1.5-gui-audio-tools
rm -f releases/pluto-plus-sdr-toolkit-v1.5-gui-audio-tools.zip

./tools/package_windows_release.sh v1.5-gui-audio-tools
cmd.exe /c releases\\pluto-plus-sdr-toolkit-v1.5-gui-audio-tools\\launchers\\start_session_gui.cmd
```

Test these buttons:

```text
Audio Menu
Audio Report
Sessions Folder
```

## What the installer changes

It writes:

```text
gui/PlutoGuiStarter_v3_RepoLayout/AudioToolsIntegration.cs
```

It backs up:

```text
gui/PlutoGuiStarter_v3_RepoLayout/MainWindow.xaml.cs.bak_audio_tools
```

It inserts after `InitializeComponent();`:

```csharp
AudioToolsIntegration.Attach(this);
```

The helper supports both layouts:

```text
Repo/dev: launchers\\run_audio_menu.cmd + build\\native\\pluto_audio_monitor.exe
Release:  launchers\\run_audio_menu.cmd + bin\\native\\pluto_audio_monitor.exe
```
