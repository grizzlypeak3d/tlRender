#!/bin/sh

# Install what a continuous integration runner needs. Extras are named on the
# command line rather than read from the environment, because what to build is
# now a config file and the environment no longer carries it: an unset variable
# here was a shell error, not a skipped branch.
#
# Usage: sh setup-gha.sh [extra ...]   where extra is "lcov" or "qt6"

set -x

sudo apt-get update

# OpenGL, ALSA and PulseAudio are needed whatever is being built.
sudo apt-get install xorg-dev libglu1-mesa-dev mesa-common-dev mesa-utils xvfb
xvfb-run glxinfo
sudo apt-get install libasound2-dev libpulse-dev

for EXTRA in "$@"; do
    case $EXTRA in
        lcov)
            sudo apt-get install lcov
            ;;
        qt6)
            sudo apt-get install qt6-base-dev qt6-5compat-dev qt6-declarative-dev qt6-svg-dev
            ;;
        *)
            echo "setup-gha.sh: unknown extra: $EXTRA" >&2
            exit 1
            ;;
    esac
done
