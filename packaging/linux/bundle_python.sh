#!/usr/bin/env bash
# Adds Orca's bundled Python runtime to a `cmake --install` tree for the /opt/orcaslicer packages
# (.deb, LinuxDir tarball, Fedora RPM). Orca 2.5 embeds Python for plug-ins and links libpython,
# but its Linux install rules only stage the runtime for Flatpak (src/CMakeLists.txt), so without
# this the installed orca-slicer needs a system libpython of the same version.
#   packaging/linux/bundle_python.sh <deps>/usr/local/libpython <install-dir>
# The runtime goes to <install-dir>/lib/python, where find_bundled_python_home() looks first (the
# AppImage uses the same place), and <install-dir>/libpython points at it for the RUNPATH the
# installed binary has ($ORIGIN/../libpython/lib).
set -euo pipefail

src="$1"
dst="$(cd "$2" && pwd)"
test -d "$src/lib" || { echo "bundle_python.sh: no Python runtime in $src" >&2; exit 1; }

mkdir -p "$dst/lib"
rm -rf "$dst/lib/python" "$dst/libpython"
cp -a "$src" "$dst/lib/python"
ln -s lib/python "$dst/libpython"

# These scripts start the interpreter by its path on the build machine, so they can't run from
# the package; Orca installs plug-in packages with uv, not pip.
rm -f "$dst"/lib/python/bin/{2to3,idle3,pydoc3,pip3}*
# Executable stdlib files carry #! lines (/usr/local/bin/python, the build path, ...) that
# packaging tools turn into dependencies. Python imports them; nothing runs them directly.
find "$dst/lib/python/lib" -type f ! -name '*.so*' -perm /111 -exec chmod a-x {} +
# libpython and the extension modules keep a RUNPATH to the deps folder on the build machine
# (only the interpreter gets a relative one). They need only system libraries (the modules get
# libpython's symbols from the process), so drop it; Fedora's check-rpaths rejects such paths.
rpath_tool="$(mktemp --suffix=.cmake)"
trap 'rm -f "$rpath_tool"' EXIT
cat > "$rpath_tool" <<'EOF'
file(GLOB_RECURSE libs "${DIR}/*.so" "${DIR}/*.so.*")
foreach(lib IN LISTS libs)
    if(NOT IS_SYMLINK "${lib}")
        file(RPATH_REMOVE FILE "${lib}")
    endif()
endforeach()
EOF
cmake -DDIR="$dst/lib/python/lib" -P "$rpath_tool" >/dev/null

# The installed binary must find every library, libpython in the bundle.
app="$dst/bin/orca-slicer"
if ldd "$app" | grep 'not found'; then
    echo "bundle_python.sh: $app has unresolved libraries" >&2
    exit 1
fi
ldd "$app" | grep -F "$dst/bin/../libpython/lib/libpython" ||
    { echo "bundle_python.sh: $app does not load libpython from the bundle" >&2; exit 1; }
"$dst/lib/python/bin/python3" -c 'import encodings, json, sqlite3, ctypes; print("bundled", __import__("sys").version.split()[0])'
