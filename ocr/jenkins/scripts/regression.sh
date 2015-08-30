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
    echo "regression.sh: Setting OCR_CONFIG to ${OCR_CONFIG} =${CC}"

    DB_IMPL=$3;

    # Make a copy of the tests directory so we can run in parallel
    # with other regressions
    cp -r ${JJOB_PRIVATE_HOME}/xstack/ocr/tests ${JJOB_PRIVATE_HOME}/xstack/ocr/tests-${JJOB_ID}
    cd ${JJOB_PRIVATE_HOME}/xstack/ocr/tests-${JJOB_ID}
    TEST_OPTIONS=""

    if [[ "${ARCH}" == "x86" ]]; then
        # Also tests legacy and rt-api supports
        # These MUST be built by default for OCR x86
        TEST_OPTIONS="-ext_rtapi -ext_legacy"
    fi

    ./ocrTests ${TEST_OPTIONS} -unstablefile unstable.${ARCH}-${DB_IMPL}
    RES=$?

    #Conditionally execute to preserve logs if previous run failed.
    if [[ $RES -eq 0 ]]; then
        #TODO: disable gasnet tests for now
        if [[ "${ARCH}" != "x86-gasnet" ]]; then
            #Run performance tests as non-regression tests too
            ./ocrTests -unstablefile unstable.${ARCH}-${DB_IMPL} -perftest
            RES=$?
        fi
    fi

    exit $RES
fi
