#!/bin/sh
#

# Get the directory in which this script and the binaries are located
BINDIR="${0%/*}"


for BIN in ${BINDIR}/test/*.o; do
    echo ""
    echo "=== $BIN"
    ${BINDIR}/run.sh -m 2 -f 10 $BIN $@
done
