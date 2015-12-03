#!/bin/bash

#
# Environment check
#
if [[ -z "${SCRIPT_ROOT}" ]]; then
    echo "error: ${0} environment SCRIPT_ROOT is not defined"
    exit 1
fi

if [[ -z "$CORE_SCALING" ]]; then
    echo "error $0: Cannot find environment variable CORE_SCALING";
    exit
fi

#
# Option setup
#

NOCLEAN_OPT="no"

#
# Option Parsing and Checking
#
RUNLOG_FILE=""

if [[ -z "$RUNLOG_FILE" ]]; then
    RUNLOG_FILE="${RUNLOG_FILENAME_BASE}"
    echo "$0: Use default RUNLOG_FILE: $RUNLOG_FILE";
fi

TMPDIR=${SCRIPT_ROOT}/tmp

while [[ $# -gt 0 ]]; do
    # for arg processing debug
    #echo "Processing argument $1"
    if [[ "$1" = "-noclean" ]]; then
        shift
        NOCLEAN_OPT="yes"
    else
        # Remaining is runlog file arguments
        RUNLOG_FILE=$1
        shift
    fi
done

function toLower() {
    local  input=$1
    local  __resultvar=$2
    BASH_VER=`echo "$BASH_VERSION" | cut -b1-1`
    if [[ ${BASH_VER} -eq 4 ]]; then
        input=${input,,}
    else
        input=`echo ${input} | tr '[:upper:]' '[:lower:]'`
    fi
    eval $__resultvar="'$input'"
}


function extractMetric() {
    local  METRIC_NAME=$1
    local  CUT_FIELD=$2
    local  ANALYSIS=$3

    local metric_name_low
    toLower ${METRIC_NAME} metric_name_low

    TMP_ALL_METRIC_FILES=""
    for logFile in `ls ${RUNLOG_FILE}*`; do
        logFilename=${logFile##*/}
        TMP_METRIC_FILE=${TMPDIR}/${logFilename}.tmp
        TMP_ALL_METRIC_FILES="${TMP_ALL_METRIC_FILES} ${TMP_METRIC_FILE}"
        grep ${METRIC_NAME} $logFile | cut -d' ' -f${CUT_FIELD}-${CUT_FIELD} > ${TMP_METRIC_FILE}
    done

    # Assemble each run results
    paste ${TMP_ALL_METRIC_FILES} | tr '\t' ' ' > ${TMPDIR}/tmp-all-runlog-metric

    if [[ "$ANALYSIS" == "stddev" ]]; then
        # Aggregate results and compute stddev
        while read line
        do
            ${SCRIPT_ROOT}/utils/stddev.sh "$line"
        done < ${TMPDIR}/tmp-all-runlog-metric > ${TMPDIR}/tmp-agg-results-${metric_name_low}
    elif [[ "$ANALYSIS" == "avg" ]]; then
        # Aggregate results and compute avg
        while read line
        do
            ${SCRIPT_ROOT}/utils/avg.sh "$line"
        done < ${TMPDIR}/tmp-all-runlog-metric > ${TMPDIR}/tmp-agg-results-${metric_name_low}
    else
        cp ${TMPDIR}/tmp-all-runlog-metric ${TMPDIR}/tmp-agg-results-${metric_name_low}
    fi
}


# Utility to delete a file only when NOCLEAN is set to "no"
function deleteFiles() {
    local files=$@
    if [[ "$NOCLEAN_OPT" = "no" ]]; then
        rm -Rf $files
    fi
}

echo "RUNLOG_FILE=${RUNLOG_FILE}"

#
# Grep for a metric in each runlog file
#

extractMetric "Throughput" 4 "stddev"
extractMetric "Duration"   9 "stddev"

# Compute speed-up on the average column (first one)
AVG=`cat ${TMPDIR}/tmp-agg-results-throughput | cut -d' ' -f 1-1`
AVG=`echo ${AVG} | sed -e "s/\n/ /g"`
SPUP=`${SCRIPT_ROOT}/utils/speedup.sh "$AVG"`

# Format speed-up information
echo "$SPUP" | tr ' ' '\n' > ${TMPDIR}/tmp-agg-results-spup

# Format core-scaling information
echo "${CORE_SCALING}" | tr ' ' '\n' > ${TMPDIR}/tmp-core-scaling

# final formatting: core-scaling | avg | stddev | count | speed-up | duration | stddev | count
paste ${TMPDIR}/tmp-core-scaling ${TMPDIR}/tmp-agg-results-throughput ${TMPDIR}/tmp-agg-results-spup ${TMPDIR}/tmp-agg-results-duration | column -t

# delete left-over temporary file
deleteFiles ${TMPDIR}/tmp-all-runlog-metric ${TMPDIR}/tmp-agg-results ${TMPDIR}/tmp-agg-results-spup
deleteFiles ${TMP_ALL_METRIC_FILES}
