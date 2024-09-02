#!/bin/sh

#
# "buggy" unless fairness
#

# Get the directory in which this script and the binaries are located
BINDIR="${0%/*}"


for BIN in ${BINDIR}/test/double-relseq.o ${BINDIR}/test/deadlock.o ${BINDIR}/test/pending-release.o ${BINDIR}/test/releaseseq.o; do
    echo ""
    echo "=== $BIN"
    ${BINDIR}/run.sh $BIN $@
done
