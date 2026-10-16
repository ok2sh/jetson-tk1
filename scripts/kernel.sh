#!/bin/bash
set -euo pipefail
. /w/config.env
KV=$KERNEL_VERSION
apt-get update -qq
apt-get install -y -qq curl xz-utils gcc-arm-linux-gnueabihf make gcc bc bison flex libssl-dev libelf-dev cpio kmod patch python3 >/dev/null

# the kernel tree has case-colliding paths, so it lives in a docker volume, not on the macOS bind mount
cd /b
if [ ! -d linux-$KV ]; then
  curl -fsSLO https://cdn.kernel.org/pub/linux/kernel/v${KV%%.*}.x/linux-$KV.tar.xz
  curl -fsSL https://cdn.kernel.org/pub/linux/kernel/v${KV%%.*}.x/sha256sums.asc | grep " linux-$KV.tar.xz\$" | sha256sum -c -
  tar -xf linux-$KV.tar.xz
  rm linux-$KV.tar.xz
  cd linux-$KV
  for p in /w/kernel/patches/*.patch; do patch -p1 -s < "$p"; echo "applied $(basename "$p")"; done
else
  cd linux-$KV
fi

export ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-
make -s tegra_defconfig
./scripts/kconfig/merge_config.sh -m .config /w/kernel/tk1.config >/dev/null
make -s olddefconfig
grep -v '^#' /w/kernel/tk1.config | while read -r l; do grep -qx "$l" .config || { echo "config not applied: $l"; exit 1; }; done
make -j"$(nproc)" zImage dtbs modules > /b/kernel-build.log 2>&1 || { tail -30 /b/kernel-build.log; exit 1; }

rm -rf /b/stage && mkdir -p /b/stage/boot
make -s INSTALL_MOD_PATH=/b/stage/rootfs modules_install
rm -f /b/stage/rootfs/lib/modules/$KV/build /b/stage/rootfs/lib/modules/$KV/source
cp arch/arm/boot/zImage /b/stage/boot/zImage-$KV
cp arch/arm/boot/dts/nvidia/tegra124-jetson-tk1.dtb /b/stage/boot/tegra124-jetson-tk1-$KV.dtb
cp .config /b/stage/boot/config-$KV
mkdir -p /w/out
tar -czf /w/out/linux-$KV.tar.gz -C /b/stage boot rootfs
echo "kernel: out/linux-$KV.tar.gz"
