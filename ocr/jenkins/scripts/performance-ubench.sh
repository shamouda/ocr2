#!/bin/bash

if [ $1 == "_params" ]; then
    if [ $2 == "output" ]; then
        echo "${JJOB_PRIVATE_HOME}/xstack/ocr/tests-${JJOB_ID}/tests-log/TESTS-TestSuites.xml"
        exit 0
    fi
else
    # ARGS: ARCH CFG_FILE DB_IMPL
    ARCH=$1
    export OCR_INSTALL=${JJOB_SHARED_HOME}/xstack/ocr/install/${ARCH}/
    export PATH=${OCR_INSTALL}/bin:$PATH
    export LD_LIBRARY_PATH=${OCR_INSTALL}/lib:${LD_LIBRARY_PATH}

    CFG_FILE=$2;
    export OCR_CONFIG=${OCR_INSTALL}/config/${CFG_FILE}
    echo "$0: Setting OCR_CONFIG to ${OCR_CONFIG} =${CC}"

    DB_IMPL=$3;

    cp -r ${JJOB_PRIVATE_HOME}/xstack/ocr/tests ${JJOB_PRIVATE_HOME}/xstack/ocr/tests-${JJOB_ID}
    cd ${JJOB_PRIVATE_HOME}/xstack/ocr/tests-${JJOB_ID}/performance-tests

    # temporarily hard-coded here
    export CORE_SCALING=1
    export OCR_NUM_NODES=1
    export CUSTOM_BOUNDS="NB_INSTANCES=6000 NB_ITERS=1000"
    SCRIPT_ROOT=./scripts ./scripts/perfDriver.sh event0StickyCreate

    # Copy results to shared folder
    if [[ ! -d ${JJOB_SHARED_HOME}/xstack/ocr/tests/performance-tests ]]; then
        mkdir -p ${JJOB_SHARED_HOME}/xstack/ocr/tests/performance-tests
    fi

    cp report-* ${JJOB_SHARED_HOME}/xstack/ocr/tests/performance-tests

    exit $RES
fi
