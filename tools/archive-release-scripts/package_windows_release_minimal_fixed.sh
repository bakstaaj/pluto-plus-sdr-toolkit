#!/usr/bin/env bash
set -Eeuo pipefail

# package_windows_release_minimal_fixed.sh
#
# A minimal packager that creates the release folder, copies native exe/configs,
# and then calls create_release_launchers.sh. Use this if the full package script
# still has problems.

APP_NAME="pluto-plus-sdr-toolkit"
VERSION="${1:-$(date +%Y%m%d-%H%M%S)}"
RELEASE_NAME="${APP_NAME}-${VERSION}"

ROOT="$(pwd)"
RELEASE_DIR="${ROOT}/releases/${RELEASE_NAME}"

echo "Creating minimal release:"
echo "  ${RELEASE_DIR}"

cmake -S . -B build -G Ninja
cmake --build build

rm -rf "${RELEASE_DIR}"

mkdir -p \
    "${RELEASE_DIR}/bin/native" \
    "${RELEASE_DIR}/configs" \
    "${RELEASE_DIR}/docs" \
    "${RELEASE_DIR}/gui" \
    "${RELEASE_DIR}/sessions" \
    "${RELEASE_DIR}/launchers"

cp -v build/native/*.exe "${RELEASE_DIR}/bin/native/"

if [ -d configs ]; then
    cp -rv configs/* "${RELEASE_DIR}/configs/"
fi

if [ -f README.md ]; then
    cp -v README.md "${RELEASE_DIR}/README.md"
fi

if [ -d docs ]; then
    cp -rv docs/* "${RELEASE_DIR}/docs/" || true
fi

cat > "${RELEASE_DIR}/README_RELEASE.txt" <<'EOF'
Pluto+ SDR Windows Toolkit Release

Use the launchers folder to start scans or GUIs.
Generated session outputs are written to sessions.
EOF

./tools/create_release_launchers.sh "${RELEASE_DIR}"

(
    cd releases
    rm -f "${RELEASE_NAME}.zip"
    if command -v zip >/dev/null 2>&1; then
        zip -r "${RELEASE_NAME}.zip" "${RELEASE_NAME}"
    else
        powershell.exe -NoProfile -Command "Compress-Archive -Path '${RELEASE_NAME}' -DestinationPath '${RELEASE_NAME}.zip' -Force"
    fi
)

echo
echo "Minimal release complete:"
echo "  ${RELEASE_DIR}"
echo "  ${ROOT}/releases/${RELEASE_NAME}.zip"
