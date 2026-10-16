#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
VOL=jetson-tk1-build
IMAGE=debian:trixie

step() { docker run --rm $2 -v "$VOL:/b" -v "$PWD:/w" "$IMAGE" "/w/scripts/$1.sh"; }

for t in ${*:-tegrarcm kernel uboot rootfs image}; do
  echo "=== $t"
  case $t in
    tegrarcm) scripts/tegrarcm.sh ;;
    kernel|uboot|image) step "$t" "" ;;
    rootfs) step rootfs --privileged ;;
    clean) docker volume rm -f "$VOL" >/dev/null; rm -rf out build tools ;;
    *) echo "usage: $0 [tegrarcm|kernel|uboot|rootfs|image|clean]..."; exit 1 ;;
  esac
done
