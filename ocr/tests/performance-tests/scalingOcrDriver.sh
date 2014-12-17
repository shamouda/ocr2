#!/bin/bash

if [[ $# -lt 1 ]]
then
    echo "Usage: $0 benchmarkName"
    exit 1
fi

PROG=$1

if [[ -z "$CUSTOM_BOUNDS" ]]; then

    if [[ "$#" -eq 2 ]]; then
        SWEEP_CFG=$2
    fi

    if [[ -z "${SWEEP_CFG}" ]]; then
        SWEEP_CFG=$PWD/configSweep/${PROG}.sweep
    fi

    if [[ ! -f ${SWEEP_CFG} ]]; then
        SWEEP_CFG=$PWD/configSweep/default.sweep
        echo "No sweep configuration file for ${PROG}, default to ${SWEEP_CFG}"
    fi

    echo "Loading sweep configuration: $SWEEP_CFG"

    while read -r line
    do
        echo "Executing sweep configuration: $line"
        export CUSTOM_BOUNDS=$line
        make -f Makefile.ocr PROG=ocr/$PROG
    done < "${SWEEP_CFG}"

else
    make -f Makefile.ocr PROG=ocr/$PROG
fi
