#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)"
OUTPUT_TAR="${1:-$SCRIPT_DIR/windows-wsl2-rootfs.tar}"
BASE_IMAGE="${PJARCZAK_WSL_ROOTFS_IMAGE:-ubuntu:24.04}"

if ! command -v docker >/dev/null 2>&1; then
    echo "docker not found. Install Docker or provide a prebuilt windows-wsl2-rootfs.tar." >&2
    exit 1
fi

mkdir -p "$(dirname -- "$OUTPUT_TAR")"

CONTAINER_NAME="pjarczak-bambu-rootfs-$(date +%s)-$$"
cleanup() {
    docker rm -f "$CONTAINER_NAME" >/dev/null 2>&1 || true
}
trap cleanup EXIT

# Runtime libraries the vendor's Linux plug-in loads that the base image lacks:
# libBambuSource.so dlopen()s libagora_rtc_sdk.so for Agora camera streams
# (e.g. X2D), and that links X11/libdrm. Without them the stream fails with
# "BambuTunnelAgora createAgoraRtcEngine failed". Check new plug-in releases
# with ldd against this list.
RUNTIME_PACKAGES="libx11-6 libxext6 libxfixes3 libxdamage1 libdrm2 zlib1g"

docker pull "$BASE_IMAGE" >/dev/null
docker run --name "$CONTAINER_NAME" "$BASE_IMAGE" /bin/sh -c "
    set -e
    apt-get update
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends $RUNTIME_PACKAGES
    rm -rf /var/lib/apt/lists/*
"

rm -f "$OUTPUT_TAR"
docker export "$CONTAINER_NAME" -o "$OUTPUT_TAR"

if [[ ! -s "$OUTPUT_TAR" ]]; then
    echo "failed to create rootfs tar: $OUTPUT_TAR" >&2
    exit 1
fi

echo "WSL rootfs created:"
echo "  $OUTPUT_TAR"
