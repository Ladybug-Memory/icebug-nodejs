#!/usr/bin/env bash
# download-icebug.sh — fetch the platform-specific icebug prebuilt into vendor/
# Usage: ./scripts/download-icebug.sh [version]
#   version  optional tag, e.g. "12.4" (default: latest)
set -euo pipefail

REPO="Ladybug-Memory/icebug"
VENDOR_DIR="$(cd "$(dirname "$0")/.." && pwd)/vendor"

# ---------------------------------------------------------------------------
# Resolve version tag
# ---------------------------------------------------------------------------
if [[ "${1:-}" != "" ]]; then
  TAG="$1"
  BASE_URL="https://github.com/${REPO}/releases/download/${TAG}"
else
  # Follow the /releases/latest redirect to discover the real tag
  LATEST_URL="https://github.com/${REPO}/releases/latest"
  REDIRECT=$(curl -fsSL -o /dev/null -w '%{url_effective}' "$LATEST_URL")
  TAG="${REDIRECT##*/}"          # strip everything up to the last /
  BASE_URL="https://github.com/${REPO}/releases/download/${TAG}"
fi

echo "icebug release: ${TAG}"

# ---------------------------------------------------------------------------
# Detect platform → asset name
# ---------------------------------------------------------------------------
OS_RAW="$(uname -s)"
ARCH_RAW="$(uname -m)"

case "${OS_RAW}" in
  Darwin)              OS="macos" ;;
  Linux)               OS="linux" ;;
  MINGW*|MSYS*|CYGWIN*) OS="win" ;;
  *)
    echo "error: unsupported OS '${OS_RAW}'" >&2
    exit 1
    ;;
esac

case "${ARCH_RAW}" in
  arm64|aarch64) ARCH="arm64" ;;
  x86_64)
    ARCH="x86_64"
    [[ "${OS}" == "win" ]] && ARCH="amd64"
    ;;
  *)
    echo "error: unsupported architecture '${ARCH_RAW}' on ${OS_RAW}" >&2
    exit 1
    ;;
esac

if [[ "${OS}" == "win" ]]; then
  ASSET="icebug-${OS}-${ARCH}.zip"
else
  ASSET="icebug-${OS}-${ARCH}.tar.gz"
fi

DOWNLOAD_URL="${BASE_URL}/${ASSET}"
ARCHIVE="${VENDOR_DIR}/${ASSET}"

echo "Asset:  ${ASSET}"
echo "URL:    ${DOWNLOAD_URL}"
echo "Vendor: ${VENDOR_DIR}"

# ---------------------------------------------------------------------------
# Download
# ---------------------------------------------------------------------------
mkdir -p "${VENDOR_DIR}"
echo "Downloading…"
curl -fSL --progress-bar -o "${ARCHIVE}" "${DOWNLOAD_URL}"

# ---------------------------------------------------------------------------
# Extract into vendor/  (wipe old include/ and lib/ first for a clean unpack)
# ---------------------------------------------------------------------------
echo "Extracting…"
rm -rf "${VENDOR_DIR}/include" "${VENDOR_DIR}/lib"

if [[ "${ASSET}" == *.zip ]]; then
  unzip -q "${ARCHIVE}" -d "${VENDOR_DIR}"
else
  tar -xzf "${ARCHIVE}" -C "${VENDOR_DIR}"
fi

# ---------------------------------------------------------------------------
# Clean up the archive
# ---------------------------------------------------------------------------
rm "${ARCHIVE}"

echo "Done. Contents of ${VENDOR_DIR}:"
ls "${VENDOR_DIR}"
