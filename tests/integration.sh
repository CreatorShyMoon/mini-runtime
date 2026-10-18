#!/usr/bin/env bash
set -euo pipefail

bin="${1:?usage: integration.sh <path-to-mini-ctr>}"
here="$(cd "$(dirname "$0")" && pwd)"
repo="$(dirname "$here")"

if [ "$(id -u)" -ne 0 ]; then
    echo "SKIP: integration test requires root (run with --privileged)"
    exit 0
fi

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
rootfs="$tmp/rootfs"

bash "$repo/scripts/mkrootfs.sh" "$rootfs" >/dev/null

run() { "$bin" run --rootfs "$rootfs" "$@"; }

hostname_out="$(run --hostname mybox /bin/hostname)"
if [ "$hostname_out" != "mybox" ]; then
    echo "FAIL: expected hostname 'mybox', got '$hostname_out'"
    exit 1
fi

echo_out="$(run /bin/echo hello)"
if [ "$echo_out" != "hello" ]; then
    echo "FAIL: expected 'hello', got '$echo_out'"
    exit 1
fi

if ! run /bin/sh -c 'test -r /proc/1/status'; then
    echo "FAIL: /proc was not mounted in the container"
    exit 1
fi

if ! run /bin/sh -c 'test -c /dev/null && test -c /dev/urandom'; then
    echo "FAIL: /dev device nodes are missing"
    exit 1
fi

# Signal forwarding: SIGTERM to the runtime must reach the container's command,
# so the container exits with 128 + SIGTERM (143).
"$bin" run --rootfs "$rootfs" /bin/sleep 30 &
ctr_pid=$!
sleep 0.5
kill -TERM "$ctr_pid"
set +e
wait "$ctr_pid"
term_code=$?
set -e
if [ "$term_code" != "143" ]; then
    echo "FAIL: expected exit 143 after SIGTERM, got $term_code"
    exit 1
fi

if [ "${CTR_TEST_CGROUPS:-0}" = "1" ]; then
    run --memory 64m --pids 32 --cpus 0.5 /bin/true
fi

echo "integration OK"
