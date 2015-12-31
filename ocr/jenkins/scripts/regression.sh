#!/bin/bash

if [ $1 == "_params" ]; then
    if [ $2 == "output" ]; then
        echo "${JJOB_PRIVATE_HOME}/ocr/ocr/tests-${JJOB_ID}/tests-log/TESTS-TestSuites.xml"
        exit 0
    fi
else
    # ARGS: OCR_TYPE CFG_FILE DB_IMPL
    OCR_TYPE=$1
    export OCR_INSTALL=${JJOB_SHARED_HOME}/ocr/ocr/install/
    export PATH=${OCR_INSTALL}/bin:$PATH
    export LD_LIBRARY_PATH=${OCR_INSTALL}/lib:${LD_LIBRARY_PATH}

    CFG_FILE=$2;
    export OCR_CONFIG=${OCR_INSTALL}/share/ocr/config/${OCR_TYPE}/${CFG_FILE}
    echo "regression.sh: Setting OCR_CONFIG to ${OCR_CONFIG} =${CC}"

    DB_IMPL=$3;

    # Make a copy of the tests directory so we can run in parallel
    # with other regressions
    cp -r ${JJOB_PRIVATE_HOME}/ocr/ocr/tests ${JJOB_PRIVATE_HOME}/ocr/ocr/tests-${JJOB_ID}
    cd ${JJOB_PRIVATE_HOME}/ocr/ocr/tests-${JJOB_ID}
    TEST_OPTIONS=""

    if [[ "${OCR_TYPE}" == "x86" ]]; then
        # Also tests legacy and rt-api supports
        # These MUST be built by default for OCR x86
        TEST_OPTIONS="-ext_rtapi -ext_legacy -ext_params_evt -ext_counted_evt -ext_channel_evt"
    fi

    OCR_TYPE=${OCR_TYPE} ./ocrTests ${TEST_OPTIONS} -unstablefile unstable.${OCR_TYPE}-${DB_IMPL}
    RES=$?

    #Conditionally execute to preserve logs if previous run failed.
    if [[ $RES -eq 0 ]]; then
        #TODO: disable performance test for x86-gasnet and tg-x86
        if [[ "${OCR_TYPE}" != "x86-gasnet" && "${OCR_TYPE}" != "tg-x86" ]]; then
            #Run performance tests as non-regression tests too
            OCR_TYPE=${OCR_TYPE} ./ocrTests -unstablefile unstable.${OCR_TYPE}-${DB_IMPL} -perftest
            RES=$?
        fi
    fi

    exit $RES
fi
