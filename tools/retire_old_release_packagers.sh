#!/usr/bin/env bash
set -Eeuo pipefail

# retire_old_release_packagers.sh
#
# Optional cleanup helper. It moves older experimental packaging scripts into
# tools/archive-release-scripts/ so the repo has one clear release path.

mkdir -p tools/archive-release-scripts

for f in \
    tools/package_windows_release_minimal_fixed.sh
do
    if [ -f "$f" ]; then
        echo "Archiving $f"
        mv "$f" tools/archive-release-scripts/
    fi
done

echo
echo "Current recommended release scripts:"
echo "  tools/package_windows_release.sh"
echo "  tools/create_release_launchers.sh"
