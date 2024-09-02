#!/bin/sh

#
# Prepare libbacktrace
#

set -e

git clone https://github.com/ianlancetaylor/libbacktrace.git
cd libbacktrace
./configure
make
