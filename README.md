# Jetson TK1: mainline Linux + Debian 13

## Quickstart

Requires macOS with Docker Desktop and Homebrew.

```sh
./build.sh
```

1. Connect the board's micro-USB port to the Mac.
2. Enter recovery mode: hold FORCE RECOVERY, tap RESET, release FORCE RECOVERY.
3. `sudo ./flash.sh` (erases the eMMC)
4. Press RESET, then `ssh pi@jetson-tk1.local` (password `raspberry`).

If flashing fails with "Operation not permitted", give your terminal Full Disk Access.

## Features

- Linux 7.2.11, U-Boot v2026.10, Debian 13 trixie armhf
- GPU: OpenGL 4.3, GLES 3.2, OpenCL 3.0 (`RUSTICL_ENABLE=nouveau`), clock scaling 72-852 MHz
- micro-USB networking and serial console, works with macOS Internet Sharing
- mDNS (`jetson-tk1.local`)
- Gigabit Ethernet, USB 3.0, UART console on ttyS0
- 2 GB zram swap
- Rootfs expands to fill the eMMC on first boot
- GPU test suite in `gputest/`

## Build

```sh
./build.sh tegrarcm kernel uboot rootfs image
./build.sh clean
```

Settings are in `config.env`.
