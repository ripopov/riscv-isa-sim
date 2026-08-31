#!/bin/bash
# One-shot: fetch sources, build everything, run the benchmark.
set -e
cd "$(dirname "$0")"
./00-fetch.sh
./00-spike.sh
./01-busybox-initramfs.sh
./02-linux.sh
./03-opensbi.sh
./03b-dtb.sh
./04-run.sh
./bench.sh 7
