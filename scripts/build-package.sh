#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

MODE="releasedbg"
CLEAN=0
PLUGIN_NAME="SFSCore"
LEGACY_PLUGIN_NAME="SkyrimFittingSystem"
MOD_NAME="Skyrim Fitting System"
DATA_SRC_DIR="${REPO_ROOT}/data"
DIST_DIR="${REPO_ROOT}/dist"
STAGE_DIR="${DIST_DIR}/.stage"
STAGE_DATA_DIR="${STAGE_DIR}/Data"
BUILD_ROOT="${REPO_ROOT}/build"

while (($#)); do
    case "$1" in
        --clean)
            CLEAN=1
            ;;
        *)
            echo "Usage: $0 [--clean]" >&2
            exit 2
            ;;
    esac
    shift
done

if ! command -v powershell.exe >/dev/null 2>&1; then
    echo "powershell.exe is required to invoke the Windows toolchain from WSL." >&2
    exit 1
fi

if ! command -v wslpath >/dev/null 2>&1; then
    echo "wslpath is required to convert paths for Windows tools." >&2
    exit 1
fi

SFS_BUILD_VERSION_STRING="$("${SCRIPT_DIR}/version.sh" --display)"
SFS_BUILD_VERSION="$("${SCRIPT_DIR}/version.sh" --numeric)"
VERSION="${SFS_BUILD_VERSION_STRING}"
ARCHIVE_NAME="${MOD_NAME} v${VERSION}.zip"
ARCHIVE_PATH="${DIST_DIR}/${ARCHIVE_NAME}"

WIN_ARCHIVE_PATH="$(wslpath -w "${ARCHIVE_PATH}")"
WIN_STAGE_DIR="$(wslpath -w "${STAGE_DIR}")"
FLAT_PLUGIN_SRC="${BUILD_ROOT}/v${SFS_BUILD_VERSION}/windows/x64/${MODE}/${PLUGIN_NAME}.dll"

if ((CLEAN)); then
    rm -rf "${REPO_ROOT}/build" "${REPO_ROOT}/.xmake" "${DIST_DIR}"
fi

rm -rf "${STAGE_DIR}"
mkdir -p "${STAGE_DATA_DIR}"

if [[ -d "${DATA_SRC_DIR}" ]]; then
    cp -R "${DATA_SRC_DIR}/." "${STAGE_DATA_DIR}/"
    # The runtime DLL is supplied by the fresh SE/AE build below. Never let a
    # locally deployed data artifact become a stale fallback in the archive.
    rm -f "${STAGE_DATA_DIR}/SKSE/Plugins/${PLUGIN_NAME}.dll" \
          "${STAGE_DATA_DIR}/SKSE/Plugins/${PLUGIN_NAME}.pdb" \
          "${STAGE_DATA_DIR}/SKSE/Plugins/${LEGACY_PLUGIN_NAME}.dll" \
          "${STAGE_DATA_DIR}/SKSE/Plugins/${LEGACY_PLUGIN_NAME}.pdb" \
          "${STAGE_DATA_DIR}/SKSE/Plugins/000_SkyrimFittingSystem.dll" \
          "${STAGE_DATA_DIR}/SKSE/Plugins/000_SkyrimFittingSystem.pdb" \
          "${STAGE_DATA_DIR}/SKSE/Plugins/000_SFSRaceMenuBridge.dll" \
          "${STAGE_DATA_DIR}/SKSE/Plugins/000_SFSRaceMenuBridge.pdb"
fi

BUILD_ARGS=()
if ((CLEAN)); then
    BUILD_ARGS+=("--clean")
fi
BUILD_ARGS+=("$MODE")
"${SCRIPT_DIR}/build.sh" "${BUILD_ARGS[@]}"

if [[ ! -f "${FLAT_PLUGIN_SRC}" ]]; then
    echo "Build succeeded but ${FLAT_PLUGIN_SRC} was not found." >&2
    exit 1
fi
mkdir -p "${STAGE_DATA_DIR}/SKSE/Plugins"
cp "${FLAT_PLUGIN_SRC}" "${STAGE_DATA_DIR}/SKSE/Plugins/${PLUGIN_NAME}.dll"
if find "${STAGE_DATA_DIR}" -type f -iname '*.pdb' -print -quit | grep -q .; then
    echo "Debug symbols must not enter the runtime archive." >&2
    exit 1
fi

mkdir -p "${DIST_DIR}"
rm -f "${ARCHIVE_PATH}"

POWERSHELL_ZIP_CMD="
\$ErrorActionPreference = 'Stop'
if (Test-Path -LiteralPath '$WIN_ARCHIVE_PATH') {
  Remove-Item -LiteralPath '$WIN_ARCHIVE_PATH' -Force
}
Compress-Archive -Path '$WIN_STAGE_DIR\\*' -DestinationPath '$WIN_ARCHIVE_PATH' -CompressionLevel Optimal
"
powershell.exe -NoProfile -Command "${POWERSHELL_ZIP_CMD}"

echo "Built ${PLUGIN_NAME} (${MODE})"
echo "Packaged mod to ${ARCHIVE_PATH}"
