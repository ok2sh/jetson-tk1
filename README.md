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

Power the board from the 12 V barrel jack, then connect over micro-USB or Ethernet.

- **micro-USB:** The flashing port also provides USB networking and a serial console (`/dev/cu.usbmodem*`, 115200 baud). The Mac and board will communicate over a link-local address with hostname `jetson-tk1.local`.
- **Ethernet:** Connect to a network with DHCP. The board will be reachable at `jetson-tk1.local`.

To share your Mac's internet over micro-USB, turn on Internet Sharing in System Settings > General > Sharing and select "Jetson TK1". The board will get an address in 192.168.2.0/24.

A serial console is also available on the UART header (ttyS0, 115200 baud).

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
