# Fedora RPM from source

`orcaslicer-bmcu.spec.in` builds OrcaSlicer PJB from source the same way the release RPMs are built
(`build_linux.sh -drlL` for the bundled dependencies, then Orca with Ninja), installs it in
`/opt/orcaslicer` and adds the `orca-slicer-bmcu` launcher. `make_srpm.sh` turns it into a source
RPM made from the committed `HEAD`; the release publishes that SRPM next to the RPMs, and CI
(`build_rpm_fedora.yml`) rebuilds it for each Fedora release it ships.

`build_rpms.sh` does the same on your own machine, each release in a `fedora:<release>` container
(`Dockerfile` here):

```sh
packaging/fedora/build_rpms.sh            # out/fedora/srpm/orcaslicer-bmcu-<version>-1.src.rpm
packaging/fedora/build_rpms.sh 42 43 44   # also out/fedora/fedora<release>/*.rpm
```

- It uses docker, or podman when docker is not installed (`CONTAINER_CLI=podman` to choose).
- Each release takes about two hours on a 32-core / 32 GB machine and uses most of its RAM,
  so the releases are built one after another. Images with the build dependencies are kept
  (`orcaslicer-bmcu-build:fc<release>`), and the dependency downloads are kept in the
  `orcaslicer-bmcu-dl` volume.
- Behind a proxy, the proxy variables are passed through; set `EXTRA_CA_CERT=<pem>` if the proxy
  re-signs TLS and `CONTAINER_ARGS="--network host"` if it only listens on the host.

The build downloads the dependency sources, so it needs network access. With the SRPM elsewhere:
`rpmbuild --rebuild <srpm>` after `sudo dnf builddep <srpm>`, `mock --enable-network`, or a COPR
project with "Enable internet access during builds" turned on.

Spec options (`rpmbuild --define "<name> <value>"`):
- `orca_dl_cache <dir>`: keep the dependency downloads in `<dir>`.
- `orca_deps_prefix <dir>`: use dependencies already built from the same source with
  `build_linux.sh -drlL` (`<dir>` is their `deps/build/OrcaSlicer_dep`) instead of building them.
  CI uses this with its cached dependencies.
