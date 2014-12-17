#!/bin/bash

if [[ -z "$SCRIPT_ROOT" ]]; then
    echo "SCRIPT_ROOT environment variable is not defined"
    exit 1
fi

if [[ ! -d "$SCRIPT_ROOT/tmp" ]]; then
    mkdir -p ${SCRIPT_ROOT}/tmp
fi

if [[ $# != 1 ]]; then
    echo "usage: $0 logfile"
    exit 1
fi

LOGFILE=$1

#
# Setting up temporary files
#

TMP_FOLDER=${SCRIPT_ROOT}/tmp
WU_FILE=${TMP_FOLDER}/tmp.wu
DATA_FILE=${TMP_FOLDER}/tmp.data
DATA_BUFFER_FILE=${TMP_FOLDER}/tmp.databuffer
XLABEL_FILE=${TMP_FOLDER}/tmp.xlabel

#
# Extract data for workloads
#

${SCRIPT_ROOT}/extractors/extractWorkloadUniq.sh ${LOGFILE} > ${WU_FILE}
NB_WORKLOADS=`more ${WU_FILE} | wc -l`

#
# Extract data for x-axis labels (i.e. workers used for each run)
# TODO need to formalize this a little better in the logs
#
grep "Run" ${LOGFILE} | sort | cut -d' ' -f 4-4 | uniq | sort -g > ${XLABEL_FILE}

NB_XLABEL=`more ${XLABEL_FILE} | wc -l`

#
# Extract data for throughput
#

${SCRIPT_ROOT}/extractors/extractThroughput.sh ${LOGFILE} ${NB_XLABEL} > ${DATA_BUFFER_FILE}
${SCRIPT_ROOT}/utils/transpose.sh ${DATA_BUFFER_FILE} > ${DATA_FILE}

#
# Append x-axis labels and throughput data
#

cp ${DATA_FILE} ${DATA_BUFFER_FILE}
paste ${XLABEL_FILE} ${DATA_BUFFER_FILE} > ${DATA_FILE}

#
# Feed the dataset to the plotter script
#

OUTPUT_PLOT_NAME=${TMP_FOLDER}/tmp.plt

OUTPUT_IMG_FORMAT=svg
FILENAME=${LOGFILE##*/}
FILENAME_NOEXT=${FILENAME%.scaling}
TITLE="Workers scaling for ${FILENAME_NOEXT}"
IMG_NAME="plot_workers_scaling_${FILENAME_NOEXT}".${OUTPUT_IMG_FORMAT}
XLABEL="Number Of Workers"
YLABEL="Throughput (op\/s)"

${SCRIPT_ROOT}/plotters/generateMultiCurvePlt.sh ${DATA_FILE} ${WU_FILE} ${XLABEL_FILE} ${IMG_NAME} ${OUTPUT_PLOT_NAME} "${TITLE}" "${XLABEL}" "${YLABEL}"

gnuplot ${OUTPUT_PLOT_NAME}
RES=$?

if [[ $RES == 0 ]]; then
    echo "Plot generated ${IMG_NAME}"
    rm -Rf ${WU_FILE}
    rm -Rf ${DATA_FILE}
    rm -Rf ${DATA_BUFFER_FILE}
    rm -Rf ${XLABEL_FILE}
    rm -Rf ${OUTPUT_PLOT_NAME}
else
    echo "An error occured generating the plot ${IMG_NAME}"
fi
