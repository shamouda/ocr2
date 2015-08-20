#!/bin/bash

SCRIPT_NAME=${0##*/}

#
# Environment check
#
if [[ -z "${SCRIPT_ROOT}" ]]; then
    echo "error: ${SCRIPT_NAME} environment SCRIPT_ROOT is not defined"
    exit 1
fi

#
# OCR setup and run configuration
#

if [[ -z "$CORE_SCALING" ]]; then
    export CORE_SCALING="1 2 4 8 16"
fi

#
# OCR test setup
#

PROG_ARG=""
LOGDIR_OPT="no"
LOGDIR_ARG=""
NBRUN_OPT="no"
NBRUN_ARG=""
RUNLOG_OPT="no"
RUNLOG_ARG=""
REPORT_OPT="no"
REPORT_ARG=""
SWEEPFILE_OPT="no"
SWEEPFILE_ARG=""
TARGET_ARG="x86"
NOCLEAN_OPT="no"

OCR_MAKEFILE="Makefile"

#
# Option Parsing and Checking
#

while [[ $# -gt 0 ]]; do
    # for arg processing debug
    #echo "Processing argument $1"
    if [[ "$1" = "-sweepfile" && $# -ge 2 ]]; then
        shift
        SWEEPFILE_OPT="yes"
        SWEEPFILE_ARG=("$@")
        shift
        if [[ ! -f ${SWEEPFILE_ARG} ]]; then
            echo "error: ${SCRIPT_NAME} cannot find sweepfile ${SWEEPFILE_ARG}"
            exit 1
        fi
    elif [[ "$1" = "-logdir" && $# -ge 2 ]]; then
        shift
        LOGDIR_OPT="yes"
        LOGDIR_ARG=("$@")
        shift
    elif [[ "$1" = "-nbrun" && $# -ge 2 ]]; then
        shift
        NBRUN_OPT="yes"
        NBRUN_ARG=("$@")
        shift
    elif [[ "$1" = "-runlog" && $# -ge 2 ]]; then
        shift
        RUNLOG_OPT="yes"
        RUNLOG_ARG=("$@")
        shift
    elif [[ "$1" = "-report" && $# -ge 2 ]]; then
        shift
        REPORT_OPT="yes"
        REPORT_ARG=("$@")
        shift
    elif [[ "$1" = "-target" && $# -ge 2 ]]; then
        shift
        TARGET_ARG=("$@")
        shift
    elif [[ "$1" = "-noclean" ]]; then
        shift
        NOCLEAN_OPT="yes"
    elif [[ "$1" = "-help" ]]; then
        echo "usage: ${SCRIPT_NAME} [-sweepfile file] program"
        echo "       -sweepfile file    : Use the specified sweep file for the program"
        echo "       -logdir path       : Log file output folder"
        echo "       -nbrun  integer    : Number of runs per program"
        echo "       -runlog name       : Naming for run logs"
        echo "       -report name       : Naming for run reports"
        echo "       -target name       : Target to run on (x86|mpi)"
        echo "       -noclean           : Do not cleanup temporary files"
        echo "Environment variables:"
        echo "       - CUSTOM_BOUNDS: defines to use when compiling the program"
        echo "Defines resolution order:"
        echo "       - sweepfile, CUSTOM_BOUNDS, defaults.mk"
        exit 0
    else
        # Remaining program argument
        PROG_ARG=$1
        shift
    fi
done

if [[ -z "$PROG_ARG" ]]; then
    echo "error: ${SCRIPT_NAME} is missing program argument"
    exit 1
fi

if [[ -z "$LOGDIR_ARG" ]]; then
    LOGDIR="."
else
    LOGDIR=${LOGDIR_ARG}
fi

if [[ ! -e ${LOGDIR_ARG} ]]; then
    echo "${SCRIPT_NAME} create log dir ${LOGDIR}"
    mkdir -p ${LOGDIR}
fi

if [[ -z "$RUNLOG_ARG" ]]; then
    RUNLOG_ARG="${LOGDIR}/${RUNLOG_FILENAME_BASE}-${PROG_ARG}"
else
    RUNLOG_ARG="${LOGDIR}/${RUNLOG_ARG}"
fi

if [[ -z "$REPORT_ARG" ]]; then
    REPORT_ARG="${LOGDIR}/${REPORT_FILENAME_BASE}-${PROG_ARG}"
else
    REPORT_ARG="${LOGDIR}/${REPORT_ARG}"
fi

# Utility to delete a file only when NOCLEAN is set to "no"
function deleteFiles() {
    local files=$@
    if [[ "$NOCLEAN_OPT" = "no" ]]; then
        rm -Rf $files
    fi
}

# Setup default target-dependent env variable for the config
# file generator unless they are already defined in the environment
#
function defaultConfigTarget() {
    local target=$1
    case "$target" in
        x86)
            export CFGARG_GUID=${CFGARG_GUID-"PTR"}
            export CFGARG_PLATFORM=${CFGARG_PLATFORM-"X86"}
            export CFGARG_TARGET=${CFGARG_TARGET-"x86"}
            export CFGARG_BINDING=${CFGARG_BINDING-"seq"}
            export CFGARG_ALLOC=${CFGARG_ALLOC-"32"}
            export CFGARG_ALLOCTYPE=${CFGARG_ALLOCTYPE-"mallocproxy"}
        ;;
        mpi)
            export CFGARG_GUID=${CFGARG_GUID-"COUNTED_MAP"}
            export CFGARG_PLATFORM=${CFGARG_PLATFORM-"X86"}
            export CFGARG_TARGET=${CFGARG_TARGET-"mpi"}
            export CFGARG_BINDING=${CFGARG_BINDING-"seq"}
            export CFGARG_ALLOC=${CFGARG_ALLOC-"32"}
            export CFGARG_ALLOCTYPE=${CFGARG_ALLOCTYPE-"mallocproxy"}
        ;;
        *)
            echo $"Unknown target $target"
            exit 1
    esac
}

# Generates an OCR configuration file
#
# Any CFGARG_* env variables set are transformed into program
# arguments to the configuration generator, else the generator's
# default values are used.
function generateCfgFile {

    # Read all CFGARG_ environment variables and transform
    # them into config generator's arguments
    for cfgarg in `env | grep CFGARG_`; do
        #Extract argument name and value
        parsed=(${cfgarg//=/ })
        argNameU=${parsed[0]#CFGARG_}
        argNameL=${argNameU,,}
        argValue=${parsed[1]}
        #Append to generator argument list
        arg="--${argNameL} ${argValue}"
        CFG_ARGS+="$arg "
    done
    if [[ -z "${CFGARG_OUTPUT}" ]]; then
        echo "error: ${SCRIPT_NAME} The environment variable CFGARG_OUTPUT is not set"
        exit 1
    fi

    # Invoke the cfg file generator
    ${SCRIPT_ROOT}/../../../scripts/Configs/config-generator.py --remove-destination ${CFG_ARGS}
}

#
# Compile and Run Function
#

function scalingTest {
    prog=$1
    progDefines="$2"
    for cores in `echo "${CORE_SCALING}"`; do
        runInfo="NB_WORKERS=${cores} NB_NODES=${OCR_NUM_NODES}"

        # Generate the OCR CFG file
        export CFGARG_THREADS=${cores}
        export CFGARG_OUTPUT="${prog}-${cores}c.cfg"
        generateCfgFile

        if [[ ! -f ${CFGARG_OUTPUT} ]]; then
            echo "error: ${SCRIPT_NAME} Cannot find generated OCR config file ${CFGARG_OUTPUT}"
            exit 1
        fi

        if [[ -z "${BIN_GEN_DIR}" ]]; then
            BIN_GEN_DIR=build
        fi
        if [[ ! -d ${BIN_GEN_DIR} ]]; then
            mkdir -p ${BIN_GEN_DIR}
        fi

        # Compile the program with provided defines
        echo "Compiling for OCR ${prog} ${runInfo}"
        echo "${progDefines} ${runInfo} make -f ${OCR_MAKEFILE} benchmark ${BIN_GEN_DIR}/${prog} PROG=ocr/${prog}.c"
        eval ${progDefines} ${runInfo} BUILD_GEN_DIR=${BIN_GEN_DIR} make -f ${OCR_MAKEFILE} benchmark ${BIN_GEN_DIR}/${prog} PROG=ocr/${prog}.c

        # Run the program with the appropriate OCR cfg file
        echo "Run with OCR ${prog} ${runInfo}"
        make -f ${OCR_MAKEFILE} OCR_CONFIG=${CFGARG_OUTPUT} run ${BIN_GEN_DIR}/${prog}
        RES=$?

        if [[ $RES -ne 0 ]]; then
            echo "error: run failed !"
            exit 1
        fi
        # Everything went fine, delete the generated cfg file
        # unless instructed otherwise by NOCLEAN_OPT
        deleteFiles ${CFGARG_OUTPUT}
    done
}

function runTest() {
    local prog=$1
    local nbRun=$2
    local runlog=$3
    local report=$4
    local defines=$5
    let i=0;

    #TODO if this is part of a sweep we should mangle the name
    while (( $i < ${nbRun} )); do
        scalingTest ${prog} "${defines}" | tee ${runlog}-$i
        let i=$i+1;
    done
    echo "nbRun=${nbRun} OCR_NUM_NODES=${OCR_NUM_NODES}" > ${report}
    if [[ $defines = "" ]]; then
        echo "defines: defaults.mk" >> ${report}
    else
        echo "defines: ${defines}" >> ${report}
    fi
    echo "== Results ==" >> ${report}

    # Generate a report based on any filename matching ${RUNLOG_FILE}*
    reportGenOpt=""
    if [[ "$NOCLEAN_OPT" = "no" ]]; then
        reportGenOpt="-noclean"
    fi
    ${SCRIPT_ROOT}/reportGenerator.sh ${reportGenOpt} ${runlog} >> ${report}
}

echo "Running core scaling for ${PROG_ARG}"

# Setting up env variables for the cfg file generator
defaultConfigTarget ${TARGET_ARG}

if [[ "$SWEEPFILE_OPT" = "yes" ]]; then
    # use sweep file
    echo "${SCRIPT_NAME} Loading sweep configuration: $SWEEPFILE_ARG"
    let count=0
    # Used the following approach to read the sweepfile but it
    # seems that invoking mpirun somehow breaks the loop and only
    # the first line is read.
    # while read -r defines
    # done < "${SWEEPFILE_ARG}"
    readarray defineArray < "${SWEEPFILE_ARG}"
    for defines in "${defineArray[@]}"
    do
        tmp="$defines"
        defines=`echo $tmp | tr '\n' ' '`
        echo "${0%%.c} Executing sweep configuration: $defines"
        runlogFilename=${RUNLOG_ARG}-sweep${count}
        reportFilename=${REPORT_ARG}-sweep${count}
        runTest $PROG_ARG $NBRUN_ARG $runlogFilename $reportFilename "${defines}"
        let count=${count}+1
    done
elif [[ -n "$CUSTOM_BOUNDS" ]]; then
    # use provided defines
    echo "${SCRIPT_NAME} Use CUSTOM_BOUNDS: ${CUSTOM_BOUNDS}"
    runTest $PROG_ARG $NBRUN_ARG $RUNLOG_ARG $REPORT_ARG "${CUSTOM_BOUNDS}"
else
    # rely on defaults.mk
    runTest $PROG_ARG $NBRUN_ARG $RUNLOG_ARG $REPORT_ARG ""
fi

deleteFiles ${RUNLOG_ARG}*
deleteFiles ${SCRIPT_ROOT}/tmp/*

