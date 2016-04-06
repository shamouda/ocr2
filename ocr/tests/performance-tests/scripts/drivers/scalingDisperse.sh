#
# Disperse EDT Scaling Experiment driver
#
# TARGET: Single node runs on x86 and x86 distributed
# PLATFORM: Calibrated for a foobar cluster node
#

#
# Environment check
#
if [[ -z "$SCRIPT_ROOT" ]]; then
    echo "SCRIPT_ROOT environment variable is not defined"
    exit 1
fi

unset OCR_CONFIG

. ${SCRIPT_ROOT}/drivers/utils.sh

# Inherited by runProg
if [[ -z "${LOGDIR}" ]]; then
    export LOGDIR=`mktemp -d logs_scalingDisperse.XXXXX`
else
    mkdir -p ${LOGDIR}
fi

function runAll() {
    #
    # FLAT loop w/ FINISH-EDT
    #
    export NAME=edtExecuteFlatFinishSync
    export REPORT_FILENAME_EXT="-${DEPTH}d-flat-finish${EXT}"
    export CUSTOM_BOUNDS="NB_INSTANCES=${NB_INSTANCES}" #2^20
    runProg

    #
    # FLAT loop w/ LATCHes
    #
    export NAME=edtExecuteLatchSync
    export REPORT_FILENAME_EXT="-${DEPTH}d-flat-latches${EXT}"
    export CUSTOM_BOUNDS="NB_INSTANCES=${NB_INSTANCES}" #2^20
    runProg

    #
    # Hierarchical decomposition: FINISH-EDT (Top-level & Recursive)
    #
    export NAME=edtExecuteHierFinishSync
    export CUSTOM_BOUNDS="TREE_DEPTH=20 NODE_FANOUT=2 LEAF_FANOUT=2"
    export REPORT_FILENAME_EXT="-${DEPTH}d-rec-topfinish${EXT}"
    runProg

    export NAME=edtExecuteHierFinishSync
    export REPORT_FILENAME_EXT="-${DEPTH}d-rec-finish${EXT}"
    export CUSTOM_BOUNDS="TREE_DEPTH=20 NODE_FANOUT=2 LEAF_FANOUT=2"
    export CUSTOM_BOUNDS="${CUSTOM_BOUNDS} REC_SPAWN_FINISH=1"
    runProg

    export NAME=edtSeedBlocked
    export REPORT_FILENAME_EXT="-${DEPTH}d-seed${EXT}"
    export CUSTOM_BOUNDS="NB_INSTANCES=${NB_INSTANCES}" #2^20
    runProg

    export NAME=edtSeedBlocked
    export REPORT_FILENAME_EXT="-${DEPTH}d-seed-sgtpl${EXT}"
    export CUSTOM_BOUNDS="${CUSTOM_BOUNDS} SINGLE_TEMPLATE=1"
    runProg
    echo "${SCRIPT_ROOT}/plotCoreScalingMultiRun.sh ${LOGDIR}/report*${EXT}"
    ${SCRIPT_ROOT}/plotCoreScalingMultiRun.sh ${LOGDIR}/report*${EXT}
    mv comparison-graph.svg ${LOGDIR}/comparison-${NAME_EXP}${EXT}.svg
}

#
# Common Driver Arguments
#
export CFGARG_BINDING=${CFGARG_BINDING-"seq"}
export NB_RUN=${NB_RUN-3}
export NODE_SCALING=${NODE_SCALING-"1"}

if [[ ${OCR_TYPE} == "x86" ]]; then
    export CORE_SCALING=${CORE_SCALING-"1 2 4 8 16"}
else
    # For distributed MPI or GASNET need at least two workers
    export CORE_SCALING=${CORE_SCALING-"2 4 8 16"}
fi

#
# Common OCR Build arguments
#
# export CFLAGS_USER="-DINIT_DEQUE_CAPACITY=1100000 -DGUID_PROVIDER_NB_BUCKETS=2097152"
export CFLAGS_USER="${CFLAGS_USER} -DINIT_DEQUE_CAPACITY=2200000 -DGUID_PROVIDER_NB_BUCKETS=2097152"


#
# Common Benchmark Arguments
#
# export DEPTH=20
# export NB_INSTANCES=1048576

export DEPTH=21
export NB_INSTANCES=2097152


#
# Run section
#
export NAME_EXP="disperseEDT"

# default run with assertions on
# buildOcr
# export EXT="-assertOn"
# runAll

# Same with assertions off
export NO_DEBUG=yes
buildOcr
export EXT="-${OCR_TYPE}-mRun-assertOff"
runAll

${SCRIPT_ROOT}/plotCoreScalingMultiRun.sh ${LOGDIR}/report-*
mv comparison-graph.svg comparison-${OCR_TYPE}-${NAME_EXP}-all.svg
