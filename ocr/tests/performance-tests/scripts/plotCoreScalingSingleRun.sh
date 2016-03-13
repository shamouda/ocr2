#!/bin/bash

#
# Environment check
#
if [[ -z "$SCRIPT_ROOT" ]]; then
    echo "SCRIPT_ROOT environment variable is not defined"
    exit 1
fi

if [[ $# != 1 ]]; then
    echo "usage: $0 logfile"
    exit 1
fi

LOGFILE=$1
FILENAME=${LOGFILE##*/}

#
# Setting up temporary files
#

TMPDIR=`mktemp -d -p ${SCRIPT_ROOT} tmpdir.XXXXXX`
WU_FILE=${TMP_FOLDER}/tmp.wu
DATA_FILE=${TMP_FOLDER}/tmp.data
DATA_BUFFER_FILE=${TMP_FOLDER}/tmp.databuffer
XLABEL_FILE=${TMP_FOLDER}/tmp.xlabel
IGNORE_TOP_LINES=5

#
# Extract data for workloads
#

${SCRIPT_ROOT}/extractors/extractWorkloadUniq.sh ${LOGFILE} > ${WU_FILE}
NB_WORKLOADS=`more ${WU_FILE} | wc -l`
echo "Workload" > ${WU_FILE}

#
# Extract data for x-axis labels (i.e. cores used for each run)
#

COL_THROUGHPUT_ID=1
REPORT_FILES=${LOGFILE}
${SCRIPT_ROOT}/extractors/extractReportColDataPoint.sh ${COL_THROUGHPUT_ID} ${IGNORE_TOP_LINES} ${TMPDIR} ${XLABEL_FILE} "${REPORT_FILES}"

#
# Extract data for throughput
#

COL_THROUGHPUT_ID=2
REPORT_FILES=${LOGFILE}
${SCRIPT_ROOT}/extractors/extractReportColDataPoint.sh ${COL_THROUGHPUT_ID} ${IGNORE_TOP_LINES} ${TMPDIR} ${DATA_BUFFER_FILE} "${REPORT_FILES}"

#
# Append x-axis labels and throughput data
#

paste ${XLABEL_FILE} ${DATA_BUFFER_FILE} > ${DATA_FILE}

#
# Feed the dataset to the plotter script
#

OUTPUT_PLOT_NAME=${TMP_FOLDER}/tmp.plt

OUTPUT_IMG_FORMAT=svg
FILENAME=${LOGFILE##*/}
FILENAME_NOEXT=${FILENAME%.scaling}
TITLE="Core scaling for ${FILENAME_NOEXT}"
IMG_NAME="plotCoreScaling_${FILENAME_NOEXT}".${OUTPUT_IMG_FORMAT}
XLABEL="Number of cores"
YLABEL="Throughput (op\/s)"

DATA_COL_IDX=2
${SCRIPT_ROOT}/plotters/generateMultiCurvePlt.sh ${DATA_FILE} ${WU_FILE} ${XLABEL_FILE} ${IMG_NAME} ${OUTPUT_PLOT_NAME} "${TITLE}" "${XLABEL}" "${YLABEL}" ${DATA_COL_IDX}

gnuplot ${OUTPUT_PLOT_NAME}
RES=$?

if [[ $RES == 0 ]]; then
    echo "Plot generated ${IMG_NAME}"
    rm -Rf ${TMPDIR}
else
    echo "An error occured generating the plot ${IMG_NAME}"
fi
