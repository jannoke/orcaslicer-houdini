# Build environment for the PJB RPM: a Fedora release with the spec's BuildRequires installed,
# so rebuilding for the same release skips them. build_rpms.sh makes the build context
# (buildrequires.txt from the spec, ca/ with an optional extra CA certificate for a proxy).
ARG FEDORA_VERSION=44
FROM fedora:${FEDORA_VERSION}
COPY ca/ /etc/pki/ca-trust/source/anchors/
RUN update-ca-trust
COPY buildrequires.txt /tmp/buildrequires.txt
RUN dnf install -y rpm-build 'dnf-command(builddep)' $(cat /tmp/buildrequires.txt) && dnf clean all
