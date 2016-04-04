#
# Micro-benchmark driver
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

if [[ ${OCR_TYPE} = "x86" ]]; then
    export CORE_SCALING=${CORE_SCALING-1}
    export OCR_NUM_NODES=${OCR_NUM_NODES-1}
    export NB_RUN=${NB_RUN-3}
else
    # Baseline in distributed is at least two worker threads
    export CORE_SCALING=${CORE_SCALING-2}
    export OCR_NUM_NODES=${OCR_NUM_NODES-1}
    export NB_RUN=${NB_RUN-3}
fi


#
# Build OCR
#
export CFLAGS_USER="${CFLAGS_USER} -DINIT_DEQUE_CAPACITY=2500000 -DGUID_PROVIDER_NB_BUCKETS=2500000"
buildOcr


#
# Common Definitions
#

# - Generates reports under LOGDIR
# - Generates digest report in current folder
LOGDIR=`mktemp -d logs_foobar.XXXXXX`

# Fan-out for event related tests
VAL_FAN_OUT=1


#
# EDT benchmarking
#
# Need a deque as big as NB_INSTANCES if CORE_SCALING is 1
export CUSTOM_BOUNDS="NB_INSTANCES=2500000"
# Timings: 'crash #674' 1.3 0.7
# SCRIPT_ROOT=./scripts ./scripts/perfDriver.sh -logdir ${LOGDIR} -target ${OCR_TYPE} edtCreateStickySync

SCRIPT_ROOT=./scripts ./scripts/perfDriver.sh -logdir ${LOGDIR} -target ${OCR_TYPE} edtCreateFinishSync
SCRIPT_ROOT=./scripts ./scripts/perfDriver.sh -logdir ${LOGDIR} -target ${OCR_TYPE} edtCreateLatchSync

export CUSTOM_BOUNDS="NB_INSTANCES=1 NB_ITERS=16000000" # 1.5s
SCRIPT_ROOT=./scripts ./scripts/perfDriver.sh -logdir ${LOGDIR} -target ${OCR_TYPE} edtTemplate0Create
SCRIPT_ROOT=./scripts ./scripts/perfDriver.sh -logdir ${LOGDIR} -target ${OCR_TYPE} edtTemplate0Destroy

#
# Event benchmarking
#
# Event Creation
export CUSTOM_BOUNDS="NB_INSTANCES=1 NB_ITERS=10000000" # 1.3s
SCRIPT_ROOT=./scripts ./scripts/perfDriver.sh -logdir ${LOGDIR} -target ${OCR_TYPE} event0StickyCreate
SCRIPT_ROOT=./scripts ./scripts/perfDriver.sh -logdir ${LOGDIR} -target ${OCR_TYPE} event0OnceCreate
SCRIPT_ROOT=./scripts ./scripts/perfDriver.sh -logdir ${LOGDIR} -target ${OCR_TYPE} event0LatchCreate
SCRIPT_ROOT=./scripts ./scripts/perfDriver.sh -logdir ${LOGDIR} -target ${OCR_TYPE} event0CountedCreate

export CUSTOM_BOUNDS="NB_INSTANCES=1 NB_ITERS=10000000" # 1s
SCRIPT_ROOT=./scripts ./scripts/perfDriver.sh -logdir ${LOGDIR} -target ${OCR_TYPE} event0StickyDestroy
SCRIPT_ROOT=./scripts ./scripts/perfDriver.sh -logdir ${LOGDIR} -target ${OCR_TYPE} event0OnceDestroy
SCRIPT_ROOT=./scripts ./scripts/perfDriver.sh -logdir ${LOGDIR} -target ${OCR_TYPE} event0LatchDestroy
SCRIPT_ROOT=./scripts ./scripts/perfDriver.sh -logdir ${LOGDIR} -target ${OCR_TYPE} event0CountedDestroy

# Event -> EDT | Time AddDep and Satisfy
export CUSTOM_BOUNDS="NB_ITERS=3000000 FAN_OUT=${VAL_FAN_OUT}" # 2.6
SCRIPT_ROOT=./scripts ./scripts/perfDriver.sh -logdir ${LOGDIR} -target ${OCR_TYPE} event1StickyFanOutEdtAddDep
SCRIPT_ROOT=./scripts ./scripts/perfDriver.sh -logdir ${LOGDIR} -target ${OCR_TYPE} event1OnceFanOutEdtAddDep

export CUSTOM_BOUNDS="NB_ITERS=4000000 FAN_OUT=${VAL_FAN_OUT}" # 2.7s
SCRIPT_ROOT=./scripts ./scripts/perfDriver.sh -logdir ${LOGDIR} -target ${OCR_TYPE} event1StickyFanOutEdtSatisfy
SCRIPT_ROOT=./scripts ./scripts/perfDriver.sh -logdir ${LOGDIR} -target ${OCR_TYPE} event1OnceFanOutEdtSatisfy

# Event -> Event | Time AddDep and Satisfy
export CUSTOM_BOUNDS="NB_ITERS=3000000 FAN_OUT=${VAL_FAN_OUT}" # 1.2
SCRIPT_ROOT=./scripts ./scripts/perfDriver.sh -logdir ${LOGDIR} -target ${OCR_TYPE} event2StickyFanOutStickyAddDep
SCRIPT_ROOT=./scripts ./scripts/perfDriver.sh -logdir ${LOGDIR} -target ${OCR_TYPE} event2OnceFanOutOnceAddDep
SCRIPT_ROOT=./scripts ./scripts/perfDriver.sh -logdir ${LOGDIR} -target ${OCR_TYPE} event2LatchFanOutLatchAddDep

export CUSTOM_BOUNDS="NB_ITERS=4000000 FAN_OUT=${VAL_FAN_OUT}" # 1s
SCRIPT_ROOT=./scripts ./scripts/perfDriver.sh -logdir ${LOGDIR} -target ${OCR_TYPE} event2StickyFanOutStickySatisfy
SCRIPT_ROOT=./scripts ./scripts/perfDriver.sh -logdir ${LOGDIR} -target ${OCR_TYPE} event2LatchFanOutLatchSatisfy
export CUSTOM_BOUNDS="NB_ITERS=3000000 FAN_OUT=${VAL_FAN_OUT}" # 1.3s
SCRIPT_ROOT=./scripts ./scripts/perfDriver.sh -logdir ${LOGDIR} -target ${OCR_TYPE} event2OnceFanOutOnceSatisfy


#
# Nice printing
#
outfile="$PWD/digest-report"
raw_outfile="$PWD/digest-report-raw"

if [[ -e ${raw_outfile} ]]; then
    echo "Deleting old raw outputfile $raw_outfile"
    rm -Rf ${raw_outfile}
fi

echo "Average OCR operations cost for ${OCR_TYPE} with single OCR worker" > ${outfile}
echo "" >> ${outfile}

printTimePerOpNano ${LOGDIR}/report-edtCreateLatchSync ${outfile} ${raw_outfile} '* An EDT creation takes:'
printTimePerOpNano ${LOGDIR}/report-edtCreateFinishSync ${outfile} ${raw_outfile} '* An EDT creation inside a finish EDT takes:'
printTimePerOpNano ${LOGDIR}/report-edtTemplate0Create ${outfile} ${raw_outfile} '* An EDT template creation takes:'
printTimePerOpNano ${LOGDIR}/report-edtTemplate0Destroy ${outfile} ${raw_outfile} '* An EDT template destruction takes:'

printTimePerOpNano ${LOGDIR}/report-event0StickyCreate ${outfile} ${raw_outfile} '* A sticky event creation takes:'
printTimePerOpNano ${LOGDIR}/report-event0OnceCreate ${outfile} ${raw_outfile} '* A once event creation takes:'
printTimePerOpNano ${LOGDIR}/report-event0LatchCreate ${outfile} ${raw_outfile} '* A latch event creation takes:'
printTimePerOpNano ${LOGDIR}/report-event0CountedCreate ${outfile} ${raw_outfile} '* A counted event creation takes:'

printTimePerOpNano ${LOGDIR}/report-event0StickyDestroy ${outfile} ${raw_outfile} '* A sticky event destruction takes:'
printTimePerOpNano ${LOGDIR}/report-event0OnceDestroy ${outfile} ${raw_outfile} '* A once event destruction takes:'
printTimePerOpNano ${LOGDIR}/report-event0LatchDestroy ${outfile} ${raw_outfile} '* A latch event destruction takes:'
printTimePerOpNano ${LOGDIR}/report-event0CountedDestroy ${outfile} ${raw_outfile} '* A counted event destruction takes:'

printTimePerOpNano ${LOGDIR}/report-event1StickyFanOutEdtAddDep ${outfile} ${raw_outfile} "* Add dependences between a source sticky event to ${VAL_FAN_OUT} EDT's slots takes:"
printTimePerOpNano ${LOGDIR}/report-event1OnceFanOutEdtAddDep ${outfile} ${raw_outfile} "* Add dependences between a source once event to ${VAL_FAN_OUT} EDT's slots takes:"

printTimePerOpNano ${LOGDIR}/report-event1StickyFanOutEdtSatisfy ${outfile} ${raw_outfile} "* Satisfy a sticky event that has ${VAL_FAN_OUT} EDT as dependences takes:"
printTimePerOpNano ${LOGDIR}/report-event1OnceFanOutEdtSatisfy ${outfile} ${raw_outfile} "* Satisfy a once event that has ${VAL_FAN_OUT} EDT as dependences takes:"

printTimePerOpNano ${LOGDIR}/report-event2StickyFanOutStickyAddDep ${outfile} ${raw_outfile} "* Add dependences between a source sticky event to ${VAL_FAN_OUT} sticky events takes:"
printTimePerOpNano ${LOGDIR}/report-event2OnceFanOutOnceAddDep ${outfile} ${raw_outfile} "* Add dependences between a source once event to ${VAL_FAN_OUT} once events takes:"
printTimePerOpNano ${LOGDIR}/report-event2LatchFanOutLatchAddDep ${outfile} ${raw_outfile} "* Add dependences between a source latch event to ${VAL_FAN_OUT} latch events takes:"

printTimePerOpNano ${LOGDIR}/report-event2StickyFanOutStickySatisfy ${outfile} ${raw_outfile} "* Satisfy a sticky event that has ${VAL_FAN_OUT} sticky as dependences takes:"
printTimePerOpNano ${LOGDIR}/report-event2LatchFanOutLatchSatisfy ${outfile} ${raw_outfile} "* Satisfy a latch event that has ${VAL_FAN_OUT} latch as dependences takes:"
printTimePerOpNano ${LOGDIR}/report-event2OnceFanOutOnceSatisfy ${outfile} ${raw_outfile} "* Satisfy a once event that has ${VAL_FAN_OUT} once as dependences takes:"
