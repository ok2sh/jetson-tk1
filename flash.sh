#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
ROOT=$PWD
IMG=$ROOT/out/jetson-tk1.img
U=$ROOT/out/uboot
TEGRARCM=$ROOT/tools/bin/tegrarcm
[ "$(uname -s)" = Darwin ] || { echo "flash.sh supports macOS only"; exit 1; }
[ "$(id -u)" = 0 ] || { echo "run with sudo"; exit 1; }
for f in "$IMG" "$U/boot0.img" "$U/u-boot-ums.bin" "$U/jetson-tk1.bct" "$TEGRARCM"; do
  [ -e "$f" ] || { echo "missing $f, run ./build.sh first"; exit 1; }
done
if [ "${1:-}" != "-y" ]; then
  read -r -p "This erases the Jetson TK1 eMMC. Board in recovery mode and connected? [y/N] " a
  [ "$a" = y ] || exit 1
fi

echo "loading USB mass storage U-Boot via tegrarcm"
(cd "$U" && "$TEGRARCM" --bct=jetson-tk1.bct --bootloader=u-boot-ums.bin --loadaddr=0x80108000)

disk_bytes() { diskutil info "$1" | sed -n 's/.*Disk Size:.*(\([0-9]*\) Bytes).*/\1/p'; }
find_disk() {
  local d
  for d in $(diskutil list external physical | awk '/^\/dev\/disk/{print $1}'); do
    diskutil info "$d" | grep -q "Media Name: *$1\$" && { basename "$d"; return 0; }
  done
  return 1
}
for _ in $(seq 1 30); do
  UD=$(find_disk "UMS disk 0" || true)
  B0=$(find_disk "UMS disk 1" || true)
  B1=$(find_disk "UMS disk 2" || true)
  [ -n "$UD" ] && [ -n "$B0" ] && [ -n "$B1" ] && break
  sleep 2
done
[ -n "$UD" ] && [ -n "$B0" ] && [ -n "$B1" ] || { echo "ABORT: UMS disks not found"; exit 1; }
UB=$(disk_bytes "$UD"); B0B=$(disk_bytes "$B0"); B1B=$(disk_bytes "$B1")
[ "$B0B" = 4194304 ] && [ "$B1B" = 4194304 ] || { echo "ABORT: unexpected boot partition sizes $B0B $B1B"; exit 1; }
[ "$UB" -gt "$(stat -f %z "$IMG")" ] || { echo "ABORT: user area too small ($UB bytes)"; exit 1; }
echo "eMMC: user=$UD ($UB bytes) boot0=$B0 boot1=$B1"

diskutil unmountDisk "/dev/$UD" >/dev/null 2>&1 || true
echo "writing BCT + U-Boot to boot0"
dd if="$U/boot0.img" of="/dev/r$B0" bs=512 2>&1 | tail -1
echo "clearing boot1"
dd if=/dev/zero of="/dev/r$B1" bs=1m count=4 2>&1 | tail -1
echo "writing image to user area"
dd if="$IMG" of="/dev/r$UD" bs=4m 2>&1 | tail -1
# a GPT left by stock L4T would otherwise be found through its backup header
echo "clearing stale backup GPT"
dd if=/dev/zero of="/dev/r$UD" bs=512 seek=$((UB / 512 - 2048)) count=2048 2>&1 | tail -1
sync
cmp -n "$(stat -f %z "$U/boot0.img")" "$U/boot0.img" "/dev/r$B0" && echo "boot0 verify OK"
cmp -n 67108864 "$IMG" "/dev/r$UD" && echo "user area verify OK"
echo "done: press RESET on the board"
