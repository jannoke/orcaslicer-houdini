#!/usr/bin/env bash
# Makes the PJB source RPM from the committed HEAD: <out>/srpm/orcaslicer-bmcu-<version>-1.src.rpm,
# with the tarball in <out>/SOURCES and the spec in <out>/SPECS. Needs git and rpmbuild
# (build_rpms.sh runs it in a Fedora container; CI in the Fedora job's container).
#   packaging/fedora/make_srpm.sh [out]               # default out: out/fedora
#   packaging/fedora/make_srpm.sh --buildrequires     # only print the BuildRequires, one per line
set -euo pipefail

PROJECT_DIR="$(CDPATH='' cd -- "$(dirname -- "$0")/../.." && pwd)"
name=orcaslicer-bmcu

ver="$(sed -n 's/^set(SoftFever_VERSION "\(.*\)")/\1/p' "$PROJECT_DIR/version.inc")-$(sed -n 's/^set(FORK_RELEASE_LABEL "\(.*\)")/\1/p' "$PROJECT_DIR/version.inc")"
rpm_ver="$(printf '%s' "$ver" | tr -c 'A-Za-z0-9._' '_')"

# BuildRequires: what build_linux.sh -u installs on Fedora, plus the tools the CI image adds.
mapfile -t pkgs < <(sed -n '/^REQUIRED_DEV_PACKAGES=(/,/^)/{/^REQUIRED_DEV_PACKAGES=(/d;/^)/d;s/[[:space:]]//g;/^$/d;p}' \
                        "$PROJECT_DIR/scripts/linux.d/fedora")
pkgs+=(curl findutils gzip tar which)
mapfile -t pkgs < <(printf '%s\n' "${pkgs[@]}" | sort -u)
if [[ "${1:-}" == --buildrequires ]]; then
    printf '%s\n' "${pkgs[@]}"
    exit 0
fi

OUT="$(mkdir -p "${1:-$PROJECT_DIR/out/fedora}" && cd "${1:-$PROJECT_DIR/out/fedora}" && pwd)"

if [[ -n "$(git -C "$PROJECT_DIR" status --porcelain --untracked-files=no)" ]]; then
    echo "Note: uncommitted changes are not included; the source is HEAD ($(git -C "$PROJECT_DIR" rev-parse --short HEAD))."
fi
mkdir -p "$OUT/SOURCES" "$OUT/SPECS" "$OUT/srpm"
git -C "$PROJECT_DIR" archive --format=tar.gz --prefix="$name-$rpm_ver/" -o "$OUT/SOURCES/$name-$rpm_ver.tar.gz" HEAD
BR="$(printf 'BuildRequires:  %s\n' "${pkgs[@]}")" awk '$0 == "@BUILD_REQUIRES@" { print ENVIRON["BR"]; next } { print }' \
    "$PROJECT_DIR/packaging/fedora/orcaslicer-bmcu.spec.in" |
    sed -e "s/@RPM_VERSION@/$rpm_ver/g" -e "s/@VERSION@/$ver/g" -e "s/@CHANGELOG_DATE@/$(LC_ALL=C date -u '+%a %b %d %Y')/g" \
    > "$OUT/SPECS/$name.spec"

# No dist tag: the SRPM is the same for every Fedora release. Packing it needs no BuildRequires.
rpmbuild -bs --nodeps --define "_topdir $OUT/rpmbuild" --define "_sourcedir $OUT/SOURCES" --define "_srcrpmdir $OUT/srpm" \
    --define 'dist %{nil}' "$OUT/SPECS/$name.spec"
echo "SRPM: $OUT/srpm/$name-$rpm_ver-1.src.rpm"
