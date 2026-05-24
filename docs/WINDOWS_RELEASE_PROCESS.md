# Windows Release Process

The canonical release process is:

```bash
./tools/package_windows_release.sh v0.6-test
```

The script:

1. Builds native tools into `build/native/`.
2. Creates `releases/pluto-plus-sdr-toolkit-<version>/`.
3. Copies native EXEs and DLL dependencies into `bin/native/`.
4. Copies `configs/`, `docs/`, and `README.md`.
5. Publishes WPF GUIs if `dotnet` is available.
6. Creates Windows `.cmd` launchers.
7. Writes `MANIFEST.txt`.
8. Verifies native tools, configs, launchers, and GUI publish status.
9. Creates the ZIP.

## Verify release

```bash
cat releases/pluto-plus-sdr-toolkit-v0.6-test/MANIFEST.txt
ls -la releases/pluto-plus-sdr-toolkit-v0.6-test/launchers
ls -la releases/pluto-plus-sdr-toolkit-v0.6-test/gui/PlutoSessionGui
ls -la releases/pluto-plus-sdr-toolkit-v0.6-test/gui/PlutoLiveSpectrumGui
```

## GUI launchers

The GUI launchers search for any `.exe` in the published GUI folder:

```text
gui\PlutoSessionGui\*.exe
gui\PlutoLiveSpectrumGui\*.exe
```

This avoids hard-coding a single executable name.
