#!/bin/bash
set -euo pipefail
. /w/config.env
KV=$KERNEL_VERSION
apt-get update -qq && apt-get install -y -qq e2fsprogs fdisk >/dev/null
rm -rf /b/img && mkdir -p /b/img/root /b/img/k
cd /b/img
tar -xpf /w/out/rootfs.tar --numeric-owner -C root
tar -xzf /w/out/linux-$KV.tar.gz -C k
mkdir -p root/boot/extlinux root/usr/lib/modules
cp k/boot/zImage-$KV k/boot/tegra124-jetson-tk1-$KV.dtb k/boot/config-$KV root/boot/
cp -a k/rootfs/lib/modules/$KV root/usr/lib/modules/
chown -R 0:0 root/boot root/usr/lib/modules
(cd /w/overlay && find . -type f) | while read -r f; do
  install -D -o 0 -g 0 -m "$(stat -c %a "/w/overlay/$f")" "/w/overlay/$f" "root/$f"
done
ln -sf /etc/systemd/system/usb-gadget.service root/etc/systemd/system/multi-user.target.wants/usb-gadget.service
mkdir -p root/etc/systemd/system/getty.target.wants
ln -sf /usr/lib/systemd/system/serial-getty@.service root/etc/systemd/system/getty.target.wants/serial-getty@ttyGS0.service
cat > root/boot/extlinux/extlinux.conf <<EOC
default linux-$KV
menu title Jetson TK1
timeout 30

label linux-$KV
  kernel /boot/zImage-$KV
  fdt /boot/tegra124-jetson-tk1-$KV.dtb
  append root=PARTUUID=$DISK_ID-01 rootfstype=ext4 rootwait rw console=ttyS0,115200n8 console=tty0
EOC

# U-Boot's ext4 driver predates metadata_csum and orphan_file
mkfs.ext4 -q -F -L rootfs -O ^metadata_csum,^orphan_file -d root part.img "${IMAGE_SIZE_MB}M"
e2fsck -fn part.img >/dev/null
truncate -s 0 disk.img && truncate -s $((IMAGE_SIZE_MB + 1))M disk.img
printf 'label: dos\nlabel-id: 0x%s\nstart=2048, type=83\n' "$DISK_ID" | sfdisk -q disk.img
dd if=part.img of=disk.img bs=1M seek=1 conv=notrunc,sparse status=none
cp --sparse=always disk.img /w/out/jetson-tk1.img
rm -rf /b/img
(cd /w/out && sha256sum jetson-tk1.img | tee jetson-tk1.img.sha256)
