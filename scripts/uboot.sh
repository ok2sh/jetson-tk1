#!/bin/bash
set -euo pipefail
. /w/config.env
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y -qq git gcc-arm-linux-gnueabihf make gcc bc bison flex libssl-dev libgnutls28-dev uuid-dev python3 python3-dev python3-setuptools python3-pyelftools swig device-tree-compiler libncurses-dev xxd cbootimage >/dev/null

cd /b
[ -d u-boot ] || git clone -q --depth 1 -b "$UBOOT_TAG" https://source.denx.de/u-boot/u-boot.git
if [ ! -d cbootimage-configs ]; then
  git clone -q https://github.com/NVIDIA/cbootimage-configs.git
  git -C cbootimage-configs checkout -q "$CBOOTIMAGE_CONFIGS_COMMIT"
fi

M="make -C /b/u-boot ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j$(nproc)"
rm -rf /b/ub-emmc /b/ub-ums
$M -s O=/b/ub-emmc jetson-tk1_defconfig
$M -s O=/b/ub-emmc > /b/uboot-emmc.log 2>&1 || { tail -30 /b/uboot-emmc.log; exit 1; }
$M -s O=/b/ub-ums jetson-tk1_defconfig
cat /w/uboot/ums.config >> /b/ub-ums/.config
$M -s O=/b/ub-ums olddefconfig
grep -qx 'CONFIG_BOOTCOMMAND="ums 0 mmc 0,0.1,0.2"' /b/ub-ums/.config
grep -qx 'CONFIG_ENV_IS_NOWHERE=y' /b/ub-ums/.config
$M -s O=/b/ub-ums > /b/uboot-ums.log 2>&1 || { tail -30 /b/uboot-ums.log; exit 1; }

cfgdir=/b/cbootimage-configs/$(dirname "$BCT_CFG")
bct=$(basename "$BCT_CFG" .cfg)
rm -rf /b/boot0 && mkdir /b/boot0 && cd /b/boot0
cp "/b/cbootimage-configs/$BCT_CFG" "$cfgdir/jetson-tk1-emmc.img.cfg" .
cp /b/ub-emmc/u-boot-dtb-tegra.bin u-boot.bin
cbootimage -gbct -t124 "$bct.cfg" "$bct"
cbootimage -t124 jetson-tk1-emmc.img.cfg boot0.img >/dev/null

mkdir -p /w/out/uboot
cp boot0.img /w/out/uboot/boot0.img
cp "$bct" /w/out/uboot/jetson-tk1.bct
cp /b/ub-emmc/u-boot-dtb-tegra.bin /w/out/uboot/u-boot-dtb-tegra.bin
cp /b/ub-ums/u-boot-dtb-tegra.bin /w/out/uboot/u-boot-ums.bin
(cd /w/out/uboot && sha256sum boot0.img jetson-tk1.bct u-boot-dtb-tegra.bin u-boot-ums.bin)
