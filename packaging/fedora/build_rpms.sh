#!/usr/bin/env bash
# Makes the PJB source RPM and, for each Fedora release given, rebuilds it into a binary RPM in
# a fedora:<release> container (see README.md here). Output goes to out/fedora/.
#   packaging/fedora/build_rpms.sh           # only the SRPM
#   packaging/fedora/build_rpms.sh 43 44     # the SRPM, then RPMs for Fedora 43 and 44
# The releases are built one after another: each build uses most of the machine's RAM.
# The source is the committed HEAD (git archive), not uncommitted changes.
#
# Environment: CONTAINER_CLI (docker, or podman when docker is missing), OUT_DIR,
# EXTRA_CA_CERT (PEM file of a proxy's CA to trust), CONTAINER_ARGS (extra arguments for the
# container build and run commands, e.g. "--network host"). Proxy variables are passed through.
set -euo pipefail

PROJECT_DIR="$(CDPATH='' cd -- "$(dirname -- "$0")/../.." && pwd)"
HERE="$PROJECT_DIR/packaging/fedora"
OUT="${OUT_DIR:-$PROJECT_DIR/out/fedora}"
if [[ -z "${CONTAINER_CLI:-}" ]]; then
    if command -v docker >/dev/null; then CONTAINER_CLI=docker; else CONTAINER_CLI=podman; fi
fi
read -r -a extra_args <<< "${CONTAINER_ARGS:-}"
proxy_vars=(HTTP_PROXY HTTPS_PROXY NO_PROXY http_proxy https_proxy no_proxy)
name=orcaslicer-bmcu
dl_volume=orcaslicer-bmcu-dl

# Root in a rootful container owns what it writes into the output folder; hand it back.
# (Under rootless podman, container root already is the calling user.)
chown_out=""
[[ "$CONTAINER_CLI" == docker ]] && chown_out="chown -R $(id -u):$(id -g) /out"

run_container() {
    local env_args=() v
    for v in "${proxy_vars[@]}"; do env_args+=(-e "$v"); done
    "$CONTAINER_CLI" run --rm "${extra_args[@]}" "${env_args[@]}" "$@"
}

# Image with the BuildRequires installed, made from Dockerfile here; prints its name.
mkdir -p "$OUT/context/ca"
"$HERE/make_srpm.sh" --buildrequires > "$OUT/context/buildrequires.txt"
rm -f "$OUT/context/ca/"*
if [[ -n "${EXTRA_CA_CERT:-}" ]]; then cp "$EXTRA_CA_CERT" "$OUT/context/ca/extra-ca.crt"; fi
build_image() {
    local args=() v
    for v in "${proxy_vars[@]}"; do args+=(--build-arg "$v"); done
    "$CONTAINER_CLI" build "${extra_args[@]}" "${args[@]}" --build-arg "FEDORA_VERSION=$1" \
        -t "$name-build:fc$1" -f "$HERE/Dockerfile" "$OUT/context" >&2
    echo "$name-build:fc$1"
}

# 1. The SRPM. Any Fedora release can make it; use the first one asked for (its image is needed anyway).
image="$(build_image "${1:-44}")"
srpm="$(run_container -v "$PROJECT_DIR:/src:ro,z" -v "$OUT:/out:z" "$image" bash -euo pipefail -c "
    git config --global --add safe.directory /src
    /src/packaging/fedora/make_srpm.sh /out
    $chown_out" | tee >(cat >&2) | sed -n 's|^SRPM: .*/||p')"
[[ -n "$srpm" ]] || { echo "make_srpm.sh made no SRPM" >&2; exit 1; }
echo "SRPM: $OUT/srpm/$srpm"

# 2. One binary RPM per Fedora release, rebuilt from the SRPM. Dependency downloads are kept in
# the $dl_volume volume between builds.
for release in "$@"; do
    echo "=== Fedora $release: $(date -u +%H:%M) UTC"
    image="$(build_image "$release")"
    run_container -v "$OUT:/out:z" -v "$dl_volume:/dl" "$image" bash -euo pipefail -c "
        dnf builddep -y /out/srpm/$srpm  # only does something if the spec gained BuildRequires
        rpmbuild --rebuild --define '_topdir /tmp/rpm' --define 'orca_dl_cache /dl' /out/srpm/$srpm
        mkdir -p /out/fedora$release
        cp /tmp/rpm/RPMS/x86_64/*.rpm /out/fedora$release/
        $chown_out"
    ls -l "$OUT/fedora$release"
done
