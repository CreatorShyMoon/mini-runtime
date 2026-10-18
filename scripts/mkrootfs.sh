#!/usr/bin/env bash
set -euo pipefail

dest="${1:-rootfs}"

busybox_bin="$(command -v busybox || true)"
if [ -z "$busybox_bin" ]; then
    echo "error: busybox not found; install busybox-static" >&2
    exit 1
fi

rm -rf "$dest"
mkdir -p "$dest"/{bin,dev,dev/pts,dev/shm,etc,proc,root,sys,tmp}
chmod 1777 "$dest/tmp"
chmod 1777 "$dest/dev/shm"

cp "$busybox_bin" "$dest/bin/busybox"
chmod 755 "$dest/bin/busybox"

for applet in sh ls cat echo pwd mount ps sleep hostname id whoami env mkdir true false; do
    ln -sf busybox "$dest/bin/$applet"
done

printf 'container\n' >"$dest/etc/hostname"

echo "rootfs ready at $dest"
