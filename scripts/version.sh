#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

MODE="display"
while (($#)); do
    case "$1" in
        --numeric)
            MODE="numeric"
            ;;
        --display)
            MODE="display"
            ;;
        *)
            echo "Usage: $0 [--numeric|--display]" >&2
            exit 2
            ;;
    esac
    shift
done

SEMVER_REGEX='^v?([0-9]+)\.([0-9]+)\.([0-9]+)$'
PROJECT_VERSION="$(tr -d '[:space:]' < "${REPO_ROOT}/VERSION")"
if [[ ! "v${PROJECT_VERSION}" =~ ${SEMVER_REGEX} ]]; then
    echo "VERSION must contain major.minor.patch, got ${PROJECT_VERSION}" >&2
    exit 1
fi

HEAD_TAG=""
mapfile -t HEAD_TAGS < <(git -C "${REPO_ROOT}" tag --points-at HEAD --sort=-creatordate)
for tag in "${HEAD_TAGS[@]}"; do
    if [[ "${tag}" =~ ${SEMVER_REGEX} ]]; then
        HEAD_TAG="${tag}"
        break
    fi
done

DIRTY_SUFFIX=""
if [[ -n "$(git -C "${REPO_ROOT}" status --porcelain)" ]]; then
    DIRTY_SUFFIX=".dirty"
fi
SHORT_SHA="$(git -C "${REPO_ROOT}" rev-parse --short HEAD)"

if [[ -n "${HEAD_TAG}" && -z "${DIRTY_SUFFIX}" ]]; then
    BASE_VERSION="${HEAD_TAG#v}"
    DISPLAY_VERSION="${BASE_VERSION}"
else
    BASE_VERSION="${PROJECT_VERSION}"
    DISPLAY_VERSION="${BASE_VERSION}-dev+${SHORT_SHA}${DIRTY_SUFFIX}"
fi

if [[ "${MODE}" == "numeric" ]]; then
    printf '%s\n' "${BASE_VERSION}"
else
    printf '%s\n' "${DISPLAY_VERSION}"
fi
