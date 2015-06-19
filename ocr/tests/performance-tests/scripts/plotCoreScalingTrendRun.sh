#!/bin/bash

#
# Environment check
#
if [[ -z "$SCRIPT_ROOT" ]]; then
    echo "SCRIPT_ROOT environment variable is not defined"
    exit 1
fi

if [[ ! -d "$SCRIPT_ROOT/tmp" ]]; then
    mkdir -p ${SCRIPT_ROOT}/tmp
fi

if [[ $# -lt 1 ]]; then
    echo "usage: $0 logfile"
    exit 1
fi

REPORT_FILES=$@

#Extract a filename for the report: careful with regexp here, use echo
REPORT_FILENAME=`echo ${REPORT_FILES} | tr -s ' ' | cut -d' ' -f 1-1`

#
# Setting up temporary files
#

TMPDIR=${SCRIPT_ROOT}/tmp
WU_FILE=${TMPDIR}/tmp.wu
DATA_FILE=${TMPDIR}/tmp.data
DATA_BUFFER_FILE=${TMPDIR}/tmp.databuffer
XLABEL_FILE=${TMPDIR}/tmp.xlabel
IGNORE_TOP_LINES=5

#
# Extract data for workload labels (i.e. cores used for each run)
#

COL_THROUGHPUT_ID=1
${SCRIPT_ROOT}/extractors/extractReportColDataPoint.sh ${COL_THROUGHPUT_ID} ${IGNORE_TOP_LINES} ${WU_FILE} ${REPORT_FILENAME}

#
# Extract data for throughput
#

COL_THROUGHPUT_ID=2
${SCRIPT_ROOT}/extractors/extractReportColDataPoint.sh ${COL_THROUGHPUT_ID} ${IGNORE_TOP_LINES} ${DATA_BUFFER_FILE} "${REPORT_FILES}"

# Transpose result file
${SCRIPT_ROOT}/utils/transpose.sh ${DATA_BUFFER_FILE} > ${DATA_FILE}

#
# Append x-axis labels and throughput data
#

cp ${DATA_FILE} ${DATA_BUFFER_FILE}
NB_REPORTS=`echo ${REPORT_FILES} | wc -w | tr '\t' ' ' | sed -e "s/ //g"`
seq 1 ${NB_REPORTS} > ${XLABEL_FILE}
paste -d ' ' ${XLABEL_FILE} ${DATA_BUFFER_FILE} > ${DATA_FILE}

#
# Feed the dataset to the plotter script
#

REPORT_FILENAME=${REPORT_FILENAME##*/}

OUTPUT_PLOT_NAME=${TMPDIR}/tmp.plt

OUTPUT_IMG_FORMAT=svg
TITLE="Core Scaling Trend: ${REPORT_FILENAME} (${NB_REPORTS})"
IMG_NAME="trend-graph-${REPORT_FILENAME}".${OUTPUT_IMG_FORMAT}
XLABEL="Trend Run"
YLABEL="Throughput (op\/s)"

DATA_COL_IDX=2
${SCRIPT_ROOT}/plotters/generateMultiCurvePlt.sh ${DATA_FILE} ${WU_FILE} ${XLABEL_FILE} ${IMG_NAME} ${OUTPUT_PLOT_NAME} "${TITLE}" "${XLABEL}" "${YLABEL}" ${DATA_COL_IDX}

gnuplot ${OUTPUT_PLOT_NAME}
RES=$?

if [[ $RES == 0 ]]; then
    echo "Plot generated ${IMG_NAME}"
    rm -Rf ${WU_FILE} 2>/dev/null
    rm -Rf ${DATA_FILE}
    rm -Rf ${DATA_BUFFER_FILE}
    rm -Rf ${XLABEL_FILE}
    rm -Rf ${OUTPUT_PLOT_NAME}
else
    echo "An error occured generating the plot ${IMG_NAME}"
fi
