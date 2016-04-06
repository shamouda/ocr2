#!/bin/bash

if [ $1 == "_params" ]; then
    if [ $2 == "output" ]; then
        echo "${JJOB_PRIVATE_HOME}/ocr/ocr/tests-${JJOB_ID}/tests-log/TESTS-TestSuites.xml"
        exit 0
    fi
else
    # ARGS: OCR_TYPE CFG_FILE DB_IMPL
    OCR_TYPE=$1
    export OCR_INSTALL=${JJOB_SHARED_HOME}/ocr/ocr/install
    export OCR_TYPE=${OCR_TYPE}
    export PATH=${OCR_INSTALL}/bin:$PATH
    export LD_LIBRARY_PATH=${OCR_INSTALL}/lib:${LD_LIBRARY_PATH}

    CFG_FILE=$2;
    export OCR_CONFIG=${OCR_INSTALL}/share/ocr/config/${OCR_TYPE}/${CFG_FILE}
    echo "$0: Setting OCR_CONFIG to ${OCR_CONFIG} =${CC}"

    DB_IMPL=$3;

    cp -r ${JJOB_PRIVATE_HOME}/ocr/ocr/tests ${JJOB_PRIVATE_HOME}/ocr/ocr/tests-${JJOB_ID}
    cd ${JJOB_PRIVATE_HOME}/ocr/ocr/tests-${JJOB_ID}/performance-tests

    ./scripts/drivers/foobar.sh

    # Copy results to shared folder
    if [[ ! -d ${JJOB_SHARED_HOME}/ocr/ocr/tests/performance-tests ]]; then
        mkdir -p ${JJOB_SHARED_HOME}/ocr/ocr/tests/performance-tests
    fi

    cp report-* ${JJOB_SHARED_HOME}/ocr/ocr/tests/performance-tests

    for file in `find ${JJOB_PRIVATE_HOME}/ocr -name "core.*"`; do
        rm $file
    done

    exit $RES
fi
