#!/bin/bash

source "$(dirname "$0")/shared.sh"

make -j$(nproc) all daemon
