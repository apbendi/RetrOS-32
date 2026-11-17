# The container build.sh runs in (see README.md). Debian 12, as the RetrOS builder is, for its syslinux: the installer
# and the modules it loads have to be the same version, and the drive gets both from this one package set.
# syslinux's installer is built only for x86, hence linux/amd64 in docker-compose.yml.
FROM debian:12

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        ca-certificates curl gpg gpgv \
        xorriso syslinux syslinux-common mtools dosfstools fdisk && \
    rm -rf /var/lib/apt/lists/*
