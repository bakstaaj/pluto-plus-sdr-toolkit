Pluto+ SDR Windows Toolkit Release
==================================

Quick start:

  launchers\run_noaa_scan.cmd
  launchers\run_fm_scan.cmd
  launchers\start_session_gui.cmd
  launchers\start_live_spectrum_gui.cmd

Folder layout:

  bin\native\     Native command-line tools and DLL dependencies
  configs\        Scan session config profiles
  launchers\      Double-click Windows launchers
  gui\            Published WPF GUI apps, if available
  docs\           Documentation
  sessions\       Generated CSV and HTML reports go here

Generated reports and CSV files are written to:

  sessions\

Hardware assumption:

  This release assumes your Pluto+ supports RX coverage down to at least 70 MHz.
  FM broadcast, 88-108 MHz, is included as a standard test target.
