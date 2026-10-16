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

## Connecting

Power the board from the 12 V barrel jack, then connect over either:

- **micro-USB:** the port used for flashing also provides USB networking and a serial console (`/dev/cu.usbmodem*`, 115200). With no other setup, the Mac and the board talk over a link-local address, and `jetson-tk1.local` resolves.
- **Ethernet:** plug into a LAN with DHCP. The board is reachable at `jetson-tk1.local` on that network.

To give the board internet over micro-USB, open System Settings > General > Sharing > Internet Sharing, share your Mac's connection to "Jetson TK1", and turn it on. The board gets an address in 192.168.2.0/24.

The UART header (ttyS0, 115200) also has a console.

## Features

- Linux 7.2.11, U-Boot v2026.10, Debian 13 trixie armhf
- GPU: OpenGL 4.3, GLES 3.2, OpenCL 3.0 (`RUSTICL_ENABLE=nouveau`), clock scaling 72-852 MHz
- micro-USB networking and serial console
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
