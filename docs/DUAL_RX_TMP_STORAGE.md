# Dual RX + `/tmp` Storage Fallback

This package adds `pluto_dual_rx_probe` and changes storage fallback order to:

```text
sdcard -> usb -> tmpfs -> jffs2
```

With current firmware, SD still does not enumerate, so generated WAV/CSV/report output goes to:

```text
/tmp/pluto_ham_scan/sessions
```

This is volatile and disappears after reboot, but it avoids filling `/mnt/jffs2`.

## Install into repo

```bash
cd ~/sdrdev/pluto_native_test
cp /c/Users/jim/Downloads/PlutoDualRxTmpStorage/tools/install_dual_rx_tmp_storage.sh tools/
chmod +x tools/install_dual_rx_tmp_storage.sh
./tools/install_dual_rx_tmp_storage.sh
```

## Build native tools

```bash
./tools/build_native_ucrt64.sh
```

## Deploy Pluto scripts

```bash
./tools/deploy_pluto_dual_rx_tmp_storage_msys2.sh
```

All deploys use `scp -O`.

## Deploy ARM binary

Copy the ARM-built `pluto_dual_rx_probe` to:

```text
/mnt/jffs2/pluto_ham_scan/bin/pluto_dual_rx_probe
```

Then:

```bash
ssh root@192.168.2.1 "chmod +x /mnt/jffs2/pluto_ham_scan/bin/pluto_dual_rx_probe"
```

## Test storage

```sh
/mnt/jffs2/pluto_ham_scan/tools/pluto_storage_prepare.sh
/mnt/jffs2/pluto_ham_scan/tools/pluto_storage_status.sh
```

Expected:

```text
STORAGE_BACKEND: tmpfs
SESSION_DIR: /tmp/pluto_ham_scan/sessions
```

## Test dual RX

```sh
/mnt/jffs2/pluto_ham_scan/launchers/run_dual_rx_probe_storage.sh
```

Expected on the v0.33 dirty firmware:

```text
RX1 voltage0/1: OK
RX2 voltage2/3: OK
Effective RX mode: dual
```
