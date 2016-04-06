#!/bin/bash

#
# Environment check
#
if [[ -z "$SCRIPT_ROOT" ]]; then
    echo "SCRIPT_ROOT environment variable is not defined"
    exit 1
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

TMPDIR=`mktemp -d -p ${SCRIPT_ROOT} tmpdir.XXXXXX`
WU_FILE=${TMPDIR}/tmp.wu
DATA_FILE=${TMPDIR}/tmp.data
DATA_BUFFER_FILE=${TMPDIR}/tmp.databuffer
XLABEL_FILE=${TMPDIR}/tmp.xlabel
IGNORE_TOP_LINES=5

#
# Extract data for workload labels (i.e. cores used for each run)
#

COL_THROUGHPUT_ID=1
${SCRIPT_ROOT}/extractors/extractReportColDataPoint.sh ${COL_THROUGHPUT_ID} ${IGNORE_TOP_LINES} ${TMPDIR} ${WU_FILE} ${REPORT_FILENAME}

#
# Extract data for throughput
#

COL_THROUGHPUT_ID=2
${SCRIPT_ROOT}/extractors/extractReportColDataPoint.sh ${COL_THROUGHPUT_ID} ${IGNORE_TOP_LINES} ${TMPDIR} ${DATA_BUFFER_FILE} "${REPORT_FILES}"

# Transpose result file
${SCRIPT_ROOT}/utils/transpose.sh ${DATA_BUFFER_FILE} > ${DATA_FILE}

#
# Append x-axis labels and throughput data
#

cp ${DATA_FILE} ${DATA_BUFFER_FILE}
if [[ -n "${PLOT_ARG_XAXIS}" ]]; then
    rm -f ${XLABEL_FILE}
    let i=1
    for str in `echo "${PLOT_ARG_XAXIS}"`; do
        echo "${str}" >> ${XLABEL_FILE}
    done
    # echo ${PLOT_ARG_XAXIS} | tr ' ' '\n' > ${XLABEL_FILE}
else
    NB_REPORTS=`echo ${REPORT_FILES} | wc -w | tr '\t' ' ' | sed -e "s/ //g"`
    seq 1 ${NB_REPORTS} > ${XLABEL_FILE}
fi
paste -d ' ' ${XLABEL_FILE} ${DATA_BUFFER_FILE} > ${DATA_FILE}

#
# Feed the dataset to the plotter script
#

REPORT_FILENAME=${REPORT_FILENAME##*/}

OUTPUT_PLOT_NAME=${TMPDIR}/tmp.plt

OUTPUT_IMG_FORMAT=svg

PLOT_ARG_TITLE=${PLOT_ARG_TITLE-"Core Scaling Trend: ${REPORT_FILENAME} (${NB_REPORTS})"}
PLOT_ARG_IMG_NAME=${PLOT_ARG_IMG_NAME-"trend-graph-${REPORT_FILENAME}".${OUTPUT_IMG_FORMAT}}
PLOT_ARG_XLABEL=${PLOT_ARG_XLABEL-"Trend Run"}
PLOT_ARG_YLABEL=${PLOT_ARG_YLABEL-"Throughput (op\/s)"}

DATA_COL_IDX=2
${SCRIPT_ROOT}/plotters/generateMultiCurvePlt.sh ${DATA_FILE} ${WU_FILE} ${XLABEL_FILE} ${PLOT_ARG_IMG_NAME} ${OUTPUT_PLOT_NAME} "${PLOT_ARG_TITLE}" "${PLOT_ARG_XLABEL}" "${PLOT_ARG_YLABEL}" ${DATA_COL_IDX}

gnuplot ${OUTPUT_PLOT_NAME}
RES=$?

if [[ $RES == 0 ]]; then
    echo "Plot generated ${PLOT_ARG_IMG_NAME}"
    rm -Rf ${TMPDIR}
else
    echo "An error occured generating the plot ${PLOT_ARG_IMG_NAME}"
fi
