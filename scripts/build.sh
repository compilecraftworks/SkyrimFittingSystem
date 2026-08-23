#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

MODE="releasedbg"
CLEAN=0
PLUGIN_TARGET="SkyrimFittingSystem"
PLUGIN_NAME="SFSCore"
BUILD_ROOT="${REPO_ROOT}/build"

while (($#)); do
    case "$1" in
        --clean)
            CLEAN=1
            ;;
        release|debug|releasedbg)
            MODE="$1"
            ;;
        *)
            echo "Usage: $0 [--clean] [release|debug|releasedbg]" >&2
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
    echo "wslpath is required to convert the repo path for Windows xmake." >&2
    exit 1
fi

SFS_BUILD_VERSION="$("${SCRIPT_DIR}/version.sh" --numeric)"
SFS_BUILD_VERSION_STRING="$("${SCRIPT_DIR}/version.sh" --display)"
WIN_REPO_ROOT="$(wslpath -w "${REPO_ROOT}")"

if ((CLEAN)); then
    rm -rf "${REPO_ROOT}/build" "${REPO_ROOT}/.xmake"
fi

build_variant() {
    local variant_name="$1"
    shift
    local skyrim_se="$1"
    local skyrim_ae="$2"
    local skyrim_vr="$3"
    local variant_build_dir="${BUILD_ROOT}/${variant_name}"
    local win_variant_build_dir
    win_variant_build_dir="$(wslpath -w "${variant_build_dir}")"

    # A v1.4.0 upgrade must never leave a second copy of the full plugin in
    # the build output. Two differently named SFS DLLs would both be loaded by
    # SKSE and install the same hooks twice.
    rm -f "${variant_build_dir}/windows/x64/${MODE}/SkyrimFittingSystem.dll" \
          "${variant_build_dir}/windows/x64/${MODE}/SkyrimFittingSystem.pdb" \
          "${variant_build_dir}/windows/x64/${MODE}/000_SkyrimFittingSystem.dll" \
          "${variant_build_dir}/windows/x64/${MODE}/000_SkyrimFittingSystem.pdb" \
          "${variant_build_dir}/windows/x64/${MODE}/000_SFSRaceMenuBridge.dll" \
          "${variant_build_dir}/windows/x64/${MODE}/000_SFSRaceMenuBridge.pdb"

    # xmake does not always regenerate CommonLib's generated version resources
    # when only SFS_BUILD_VERSION changes. Remove the generated files so
    # incremental builds pick up the version calculated above without a full
    # clean build.
    local generated_dir="${variant_build_dir}/.gens/${PLUGIN_TARGET}/windows/x64/${MODE}"
    rm -f "${generated_dir}/commonlib-plugin.rc" \
          "${generated_dir}/commonlibsse-ng-plugin.cpp"

    local configure_cmd="
\$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath '$WIN_REPO_ROOT'
\$env:SFS_BUILD_VERSION = '$SFS_BUILD_VERSION'
\$env:SFS_BUILD_VERSION_STRING = '$SFS_BUILD_VERSION_STRING'
xmake f -P '$WIN_REPO_ROOT' -y -o '$win_variant_build_dir' --skyrim_se=${skyrim_se} --skyrim_ae=${skyrim_ae} --skyrim_vr=${skyrim_vr} -m '$MODE'
if (\$LASTEXITCODE -ne 0) {
    exit \$LASTEXITCODE
}
"
    local parallel_build_cmd="
\$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath '$WIN_REPO_ROOT'
xmake -P '$WIN_REPO_ROOT' -y
"
    local single_job_build_cmd="
\$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath '$WIN_REPO_ROOT'
xmake -P '$WIN_REPO_ROOT' -y -j 1
"
    local build_log
    build_log="$(mktemp)"

    powershell.exe -NoProfile -Command "${configure_cmd}"

    set +e
    powershell.exe -NoProfile -Command "${parallel_build_cmd}" 2>&1 | tee "${build_log}"
    local build_status=${PIPESTATUS[0]}
    set -e

    if ((build_status != 0)) &&
        grep -Eq 'D8000|UNKNOWN COMMAND-LINE ERROR' "${build_log}"; then
        echo "Parallel build hit transient MSVC D8000; retrying single-job build." >&2
        powershell.exe -NoProfile -Command "${single_job_build_cmd}"
    elif ((build_status != 0)); then
        rm -f "${build_log}"
        exit "${build_status}"
    fi

    rm -f "${build_log}"

    local plugin_src="${variant_build_dir}/windows/x64/${MODE}/${PLUGIN_NAME}.dll"
    if [[ ! -f "${plugin_src}" ]]; then
        echo "Build succeeded but ${plugin_src} was not found." >&2
        exit 1
    fi

    local win_plugin_src
    win_plugin_src="$(wslpath -w "${plugin_src}")"
    local expected_dll_version="${SFS_BUILD_VERSION}.0"
    local version_check_cmd="
\$ErrorActionPreference = 'Stop'
\$paths = @('$win_plugin_src')
foreach (\$path in \$paths) {
    \$versionInfo = (Get-Item -LiteralPath \$path).VersionInfo
    if (\$versionInfo.FileVersion -ne '$expected_dll_version' -or \$versionInfo.ProductVersion -ne '$expected_dll_version') {
        throw \"Built DLL version mismatch for \$path: expected $expected_dll_version, got FileVersion=\$(\$versionInfo.FileVersion), ProductVersion=\$(\$versionInfo.ProductVersion)\"
    }
}
"
    powershell.exe -NoProfile -Command "${version_check_cmd}"
}

build_variant "v${SFS_BUILD_VERSION}" y y n

echo "Built ${PLUGIN_NAME} (${MODE})"
echo "Version ${SFS_BUILD_VERSION_STRING}"
echo "SE/AE build: ${BUILD_ROOT}/v${SFS_BUILD_VERSION}/windows/x64/${MODE}/${PLUGIN_NAME}.dll"
