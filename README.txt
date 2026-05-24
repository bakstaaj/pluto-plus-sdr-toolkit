Pluto Release Packaging Consolidated
===================================

This package consolidates release packaging into one reliable script.

Files:
- tools/create_release_launchers.sh
- tools/package_windows_release.sh
- tools/retire_old_release_packagers.sh
- docs/WINDOWS_RELEASE_PROCESS.md

Install from MSYS2 UCRT64:

    cd ~/sdrdev/pluto_native_test

    cp /mnt/data/PlutoReleasePackagingConsolidated/tools/create_release_launchers.sh tools/
    cp /mnt/data/PlutoReleasePackagingConsolidated/tools/package_windows_release.sh tools/
    cp /mnt/data/PlutoReleasePackagingConsolidated/tools/retire_old_release_packagers.sh tools/
    cp /mnt/data/PlutoReleasePackagingConsolidated/docs/WINDOWS_RELEASE_PROCESS.md docs/

    chmod +x tools/create_release_launchers.sh
    chmod +x tools/package_windows_release.sh
    chmod +x tools/retire_old_release_packagers.sh

Optional cleanup:

    ./tools/retire_old_release_packagers.sh

Build release:

    ./tools/package_windows_release.sh v0.4-test

Verify:

    ls -la releases/pluto-plus-sdr-toolkit-v0.4-test/launchers

Commit:

    git status
    git add tools/create_release_launchers.sh tools/package_windows_release.sh tools/retire_old_release_packagers.sh docs/WINDOWS_RELEASE_PROCESS.md
    git commit -m "Consolidate Windows release packaging"
    git push
