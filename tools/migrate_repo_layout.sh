#!/usr/bin/env bash
set -euo pipefail

# Run from repository root:
#   cd ~/sdrdev/pluto_native_test
#   bash tools/migrate_repo_layout.sh

mkdir -p native/src configs launchers gui docs tools

move_if_exists() {
    local src="$1"
    local dst="$2"
    if [ -e "$src" ]; then
        if [ -e "$dst" ]; then
            echo "SKIP: $dst already exists"
        else
            echo "MOVE: $src -> $dst"
            mv "$src" "$dst"
        fi
    fi
}

copy_if_missing() {
    local src="$1"
    local dst="$2"
    if [ -e "$dst" ]; then
        echo "SKIP: $dst already exists"
    else
        echo "COPY: $src -> $dst"
        cp "$src" "$dst"
    fi
}

if [ -d src ]; then
    for f in src/*.c src/*.h; do
        [ -e "$f" ] || continue
        move_if_exists "$f" "native/src/$(basename "$f")"
    done
    rmdir src 2>/dev/null || true
fi

for f in run_*.cmd open_latest_reports.cmd clean_session_outputs.cmd; do
    [ -e "$f" ] || continue
    move_if_exists "$f" "launchers/$(basename "$f")"
done

for f in git_setup.sh git_setup.cmd; do
    [ -e "$f" ] || continue
    move_if_exists "$f" "tools/$(basename "$f")"
done

if [ -f CMakeLists.txt ] && [ ! -f CMakeLists.old-root.txt ]; then
    echo "BACKUP: CMakeLists.txt -> CMakeLists.old-root.txt"
    cp CMakeLists.txt CMakeLists.old-root.txt
fi

copy_if_missing repo-layout/CMakeLists.root.txt CMakeLists.txt
copy_if_missing repo-layout/CMakeLists.native.txt native/CMakeLists.txt
copy_if_missing repo-layout/README.md README.md
copy_if_missing repo-layout/DEVELOPMENT.md docs/DEVELOPMENT.md
copy_if_missing repo-layout/REPO_LAYOUT.md docs/REPO_LAYOUT.md
copy_if_missing repo-layout/build_native_ucrt64.sh tools/build_native_ucrt64.sh

chmod +x tools/build_native_ucrt64.sh 2>/dev/null || true

echo
echo "Repository layout migration complete."
echo "Build with:"
echo "  ./tools/build_native_ucrt64.sh"
