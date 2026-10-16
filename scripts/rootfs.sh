#!/bin/bash
set -euo pipefail
. /w/config.env
export TK1_HOSTNAME TK1_USER TK1_PASSWORD TK1_TIMEZONE DEBIAN_SUITE
apt-get update -qq
apt-get install -y -qq mmdebstrap >/dev/null
mkdir -p /w/out
mmdebstrap --mode=root --variant=minbase --architectures=armhf \
  --components="main non-free-firmware" \
  --include="$(grep -v '^#' /w/rootfs/packages | paste -sd, -)" \
  --customize-hook='sh /w/rootfs/hook.sh "$1"' \
  "$DEBIAN_SUITE" /w/out/rootfs.tar http://deb.debian.org/debian
echo "rootfs: out/rootfs.tar"
