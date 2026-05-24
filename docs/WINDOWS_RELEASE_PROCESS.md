# Windows Release Process

The release process is now intentionally simple:

```bash
./tools/package_windows_release.sh v0.4-test
```

The script:

1. Builds native tools into `build/native/`.
2. Creates `releases/pluto-plus-sdr-toolkit-<version>/`.
3. Copies native EXEs into `bin/native/`.
4. Copies MSYS2/UCRT64 DLL dependencies when `ldd` can find them.
5. Copies `configs/`, `docs/`, and `README.md`.
6. Publishes WPF GUIs if `dotnet` is available.
7. Calls `tools/create_release_launchers.sh`.
8. Verifies native tools, configs, and launchers.
9. Creates a ZIP.

## Output

```text
releases/pluto-plus-sdr-toolkit-<version>/
releases/pluto-plus-sdr-toolkit-<version>.zip
```

## Required release launchers

The package fails if these are missing:

```text
run_fm_scan.cmd
run_2m_scan.cmd
run_noaa_scan.cmd
start_session_gui.cmd
start_live_spectrum_gui.cmd
```

## First test after packaging

From Windows Explorer, open:

```text
releases\pluto-plus-sdr-toolkit-<version>\launchers\
```

Then run:

```text
run_noaa_scan.cmd
run_fm_scan.cmd
start_live_spectrum_gui.cmd
```

Generated scan results are written to:

```text
sessions\
```
