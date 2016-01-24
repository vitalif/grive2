#!/usr/bin/env bash

set -euo pipefail
IFS=$'\n\t'

cd "$(dirname $0)"

(
    mkdir -p build
    cd build
    cmake ..
    make -j4
    sudo make install
) || (
    echo -e "\nIntallation failed\n"
    exit 1
)

