#!/bin/bash

if [[ $# -lt 1 ]]
then
    echo "Usage: $0 benchmarkName"
    exit 1
fi

PROG=$1

if [[ -z "$WORKER_SCALING" ]]; then
    WORKER_SCALING="1 2 4 8 16"
fi

if [[ -z "$OCR_CONFIG_BASE" ]]; then
    OCR_CONFIG_BASE=mach-hc-1w.cfg
fi

if [[ -z "$CUSTOM_BOUNDS" ]]; then

    if [[ $# -eq 2 ]]; then
        SWEEP_CFG=$2
    fi

    if [[ -z "${SWEEP_CFG}" ]]; then
        SWEEP_CFG=$PWD/configSweep/${PROG}.sweep
    fi

    if [[ ! -f "${SWEEP_CFG}" ]]; then
        SWEEP_CFG=$PWD/configSweep/default.sweep
        echo "No sweep configuration file for ${PROG}, default to ${SWEEP_CFG}"
    fi

    echo "Loading sweep configuration: $SWEEP_CFG"

    while read -r line
    do
        echo "Executing sweep configuration: $line"
        export CUSTOM_BOUNDS=$line
        for cores in `echo "$WORKER_SCALING"`; do
            echo "Run $PROG with $cores workers"
            CFG_FILE=`echo $OCR_CONFIG_BASE | sed -e "s/1w/${cores}w/g"`
            make -f Makefile.ocr PROG=ocr/$PROG OCR_CONFIG=${OCR_INSTALL}/config/${CFG_FILE}
        done

    done < "${SWEEP_CFG}"

else
    make -f Makefile.ocr PROG=ocr/$PROG
fi
