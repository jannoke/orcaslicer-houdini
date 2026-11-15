#!/usr/bin/env bash
# Builds the Linux bridge host (pjarczak_bambu_linux_host_abi0/abi1) and packages the
# runtime into tools/pjarczak_bambu_linux_host/runtime/linux-x86_64, inside an
# ubuntu:24.04 container. That is the release the Windows WSL rootfs uses, so the
# libcrypto the packager bundles matches it. The host needs only OpenSSL and a few
# header-only libraries, not Orca's dependency build. Needs docker; takes minutes.
set -euo pipefail

PROJECT_DIR="$(CDPATH='' cd -- "$(dirname -- "$0")/../.." && pwd)"
IMAGE="${PJARCZAK_HOST_BUILD_IMAGE:-ubuntu:24.04}"

# --network host and the proxy variables let it work behind a proxy as well.
docker run --rm --network host \
    -e HTTP_PROXY -e HTTPS_PROXY -e NO_PROXY -e http_proxy -e https_proxy -e no_proxy \
    -e HOST_UID="$(id -u)" -e HOST_GID="$(id -g)" \
    -v "$PROJECT_DIR:/src" -w /src "$IMAGE" bash -euo pipefail -c '
    trap "chown -R \"\$HOST_UID:\$HOST_GID\" build tools/pjarczak_bambu_linux_host/runtime 2>/dev/null || true" EXIT
    apt-get update -qq
    DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends \
        g++ cmake ninja-build file python3 ca-certificates \
        libssl-dev nlohmann-json3-dev libeigen3-dev libboost-dev libtbb-dev libcereal-dev >/dev/null
    cmake -S tools/pjarczak_bambu_linux_host -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build build
    bash tools/pjarczak_bambu_linux_host/package_linux_host_runtime.sh build
'
