#!/bin/bash
# One-shot: fetch sources, build everything, run both benchmarks.
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
./05-coremark.sh
./06-coremark-bench.sh 7
