#!/bin/sh
set -eu

HOST_PATH="${1:-}"
RUNTIME_DIR="${2:-}"
PLUGIN_DIR="${3:-}"

# Single-quote a value for the shell that runs inside the Lima VM.
shell_quote() {
    printf "'%s'" "$(printf '%s' "$1" | sed "s/'/'\\\\''/g")"
}

if [ -n "${PJARCZAK_MAC_LINUX_WRAPPER_CMD:-}" ]; then
    exec /bin/sh -lc "$PJARCZAK_MAC_LINUX_WRAPPER_CMD"
fi

if [ -n "${PJARCZAK_MAC_DOCKER_IMAGE:-}" ]; then
    exec docker run --rm -i \
        -e PJARCZAK_BAMBU_PLUGIN_DIR=/runtime/plugin \
        -e PJARCZAK_BAMBU_NETWORK_SO=/runtime/plugin/libbambu_networking.so \
        -e PJARCZAK_BAMBU_SOURCE_SO=/runtime/plugin/libBambuSource.so \
        -e PJARCZAK_BAMBU_LIVE555_SO=/runtime/plugin/liblive555.so \
        -e PJARCZAK_EXPECTED_BAMBU_NETWORK_VERSION="${PJARCZAK_EXPECTED_BAMBU_NETWORK_VERSION:-}" \
        -v "$RUNTIME_DIR:/runtime/host:ro" \
        -v "$PLUGIN_DIR:/runtime/plugin:ro" \
        "$PJARCZAK_MAC_DOCKER_IMAGE" /runtime/host/pjarczak_bambu_linux_host
fi

if [ -n "${PJARCZAK_MAC_LIMA_INSTANCE:-}" ]; then
    exec limactl shell "$PJARCZAK_MAC_LIMA_INSTANCE" -- /bin/sh -lc \
        "export PJARCZAK_BAMBU_PLUGIN_DIR=$(shell_quote "$PLUGIN_DIR"); export PJARCZAK_BAMBU_NETWORK_SO=$(shell_quote "$PLUGIN_DIR/libbambu_networking.so"); export PJARCZAK_BAMBU_SOURCE_SO=$(shell_quote "$PLUGIN_DIR/libBambuSource.so"); export PJARCZAK_BAMBU_LIVE555_SO=$(shell_quote "$PLUGIN_DIR/liblive555.so"); exec $(shell_quote "$HOST_PATH")"
fi

echo "No macOS Linux wrapper configured. Set PJARCZAK_MAC_LINUX_WRAPPER_CMD, PJARCZAK_MAC_DOCKER_IMAGE or PJARCZAK_MAC_LIMA_INSTANCE." >&2
exit 127
