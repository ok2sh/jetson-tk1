#!/bin/sh
set -eu
R="$1"
ch() { chroot "$R" "$@"; }

echo "$TK1_HOSTNAME" > "$R/etc/hostname"
cat > "$R/etc/hosts" <<H
127.0.0.1 localhost
127.0.1.1 $TK1_HOSTNAME
::1 localhost ip6-localhost ip6-loopback
H

ch getent group plugdev >/dev/null || ch groupadd plugdev
ch useradd -m -u 1000 -s /bin/bash -G sudo,adm,dialout,video,render,plugdev "$TK1_USER"
echo "$TK1_USER:$TK1_PASSWORD" | ch chpasswd
ch passwd -l root
echo "$TK1_USER ALL=(ALL) NOPASSWD:ALL" > "$R/etc/sudoers.d/010_$TK1_USER-nopasswd"
chmod 0440 "$R/etc/sudoers.d/010_$TK1_USER-nopasswd"

mkdir -p "$R/etc/ssh/sshd_config.d"
cat > "$R/etc/ssh/sshd_config.d/10-tk1.conf" <<H
PasswordAuthentication yes
PermitRootLogin no
H
cat > "$R/etc/systemd/system/ssh-hostkeys.service" <<H
[Unit]
Description=Generate SSH host keys on first boot
ConditionPathExists=!/etc/ssh/ssh_host_ed25519_key
Before=ssh.service
[Service]
Type=oneshot
ExecStart=/usr/bin/ssh-keygen -A
RemainAfterExit=yes
[Install]
WantedBy=multi-user.target
H

echo 'LABEL=rootfs / ext4 defaults,noatime 0 1' > "$R/etc/fstab"

mkdir -p "$R/usr/local/sbin"
cat > "$R/usr/local/sbin/grow-rootfs" <<'H'
#!/bin/sh
set -eu
src=$(findmnt -n -o SOURCE /)
dev=$(lsblk -no PKNAME "$src")
part=$(cat "/sys/class/block/$(basename "$src")/partition")
growpart "/dev/$dev" "$part" || [ $? -eq 1 ]
resize2fs "$src"
touch /var/lib/grow-rootfs.done
H
chmod 755 "$R/usr/local/sbin/grow-rootfs"
cat > "$R/etc/systemd/system/grow-rootfs.service" <<H
[Unit]
Description=Grow root filesystem on first boot
ConditionPathExists=!/var/lib/grow-rootfs.done
After=local-fs.target
[Service]
Type=oneshot
ExecStart=/usr/local/sbin/grow-rootfs
ExecStartPost=/bin/systemctl disable grow-rootfs.service
RemainAfterExit=yes
[Install]
WantedBy=multi-user.target
H

sed -i 's/^# *en_US.UTF-8 UTF-8/en_US.UTF-8 UTF-8/' "$R/etc/locale.gen"
ch locale-gen
echo 'LANG=en_US.UTF-8' > "$R/etc/default/locale"
ln -sf "/usr/share/zoneinfo/$TK1_TIMEZONE" "$R/etc/localtime"
echo "$TK1_TIMEZONE" > "$R/etc/timezone"

cat > "$R/etc/apt/sources.list.d/debian.sources" <<H
Types: deb
URIs: http://deb.debian.org/debian
Suites: $DEBIAN_SUITE $DEBIAN_SUITE-updates
Components: main non-free-firmware
Signed-By: /usr/share/keyrings/debian-archive-keyring.gpg

Types: deb
URIs: http://deb.debian.org/debian-security
Suites: $DEBIAN_SUITE-security
Components: main non-free-firmware
Signed-By: /usr/share/keyrings/debian-archive-keyring.gpg
H
rm -f "$R/etc/apt/sources.list"

ch systemctl enable ssh.service ssh-hostkeys.service systemd-networkd.service systemd-resolved.service systemd-timesyncd.service avahi-daemon.service serial-getty@ttyS0.service grow-rootfs.service
rm -f "$R/etc/resolv.conf"
ln -s /run/systemd/resolve/stub-resolv.conf "$R/etc/resolv.conf"

# every flashed board must get its own machine-id and SSH host keys
: > "$R/etc/machine-id"
rm -f "$R/var/lib/dbus/machine-id" "$R"/etc/ssh/ssh_host_*
