#!/bin/bash

if [ $# -ne 2 ]; then
    echo "Usage <directory containing running times> <number of samples>"
    exit 1
fi

INPUT_FOLDER=$1
NB_SAMPLES=$2

SCRIPT_FOLDER=${JJOB_SHARED_HOME}/ocr/ocr/jenkins/scripts

RESULTS_DIR=performanceResults/NightlyPerfStatMicroBench
STATS_FILE=NightlyPerfStatMicroBench.txt
PLOT_FILE=perfStatMicroBenchTrend.png

# Create archive dirs if the framework is being run manually ( i.e. local run and not by jenkins)
if [ -z $WORKSPACE ]; then # debug
    echo "---- Local execution of Regression Framework. Creating Archive dirs ----"
    mkdir -p ${JJOB_SHARED_HOME}/../${RESULTS_DIR}
    OUT_FILE="${JJOB_SHARED_HOME}/../${RESULTS_DIR}/${BUILD_NUMBER}.csv"
else
    OUT_FILE="${WORKSPACE}/${RESULTS_DIR}/${BUILD_NUMBER}.csv"
fi

# Here we invoke our own extraction tool to spit out:
# testname1,duration
# testname2,duration
# Results of extraction are here:
for file in `ls ${INPUT_FOLDER}/report-*`; do
    # Extract name of test from report's filename, something like 'report-event0StickyCreate'
    NAME=${file##*-}
    # Extract duration
    # Warning! this relies on the run to be for one core (hence the ^1)
    DURATION=`grep "^1" ${file} | sed -e 's/  / /g' | cut -f2-2 -d' '`
    echo "${NAME},${DURATION}"
done > ${OUT_FILE}

# Generate plotGraph.py parsable stat file
if [ -z $WORKSPACE ]; then
    #  Manual execution of framework (i.e. local run and not by jenkins), args are: archiveDir outStatFile
    python ${SCRIPT_FOLDER}/perfStatFileFolderParser.py ${JJOB_SHARED_HOME}/../${RESULTS_DIR} ${JJOB_SHARED_HOME}/../${STATS_FILE}
else
    python ${SCRIPT_FOLDER}/perfStatFileFolderParser.py ${WORKSPACE}/${RESULTS_DIR} ${WORKSPACE}/${STATS_FILE}
fi
RET_VAL=$?
if [ $RET_VAL -eq 0 ]; then
    echo " ---- Successfully generated plot script parsable regression input file ----"
else
    echo " ---- Failure in generation of plot script parsable regression input file ----"
    exit $RET_VAL
fi


YMIN=0.6
YMAX=1.4

# Plot these stat files
if [ -z $WORKSPACE ]; then # debug
    #  Manual execution of framework (i.e. local run and not by jenkins)
    cat ${JJOB_SHARED_HOME}/../${STATS_FILE}
    python ${SCRIPT_FOLDER}/plotGraph.py ${JJOB_SHARED_HOME}/../${STATS_FILE} "Performance Trend Line" "Build" "Normalized Execution Time" ${YMIN} ${YMAX} "${JJOB_SHARED_HOME}/../${PLOT_FILE}"
else
    cat ${WORKSPACE}/${STATS_FILE}
    python ${SCRIPT_FOLDER}/plotGraph.py ${WORKSPACE}/${STATS_FILE} "Performance Trend Line" "Build" "Normalized Execution Time" ${YMIN} ${YMAX} "${WORKSPACE}/${PLOT_FILE}"
fi
RET_VAL=$?
if [ $RET_VAL -eq 0 ]; then
    echo " ---- Successfully generated Nightly Performance plot ----"
else
    echo " ---- Failure in generation of Nightly Performance plot ----"
    exit $RET_VAL
fi

