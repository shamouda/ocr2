#
# Micro-benchmark driver
#
# TARGET: X86 single node runs
# PLATFORM: Calibrated for a foobar cluster node

# temporarily hard-coded here
export CORE_SCALING=1
export OCR_NUM_NODES=1
export NB_RUN=3

echo "NB_RUN=${NB_RUN}"

#
# EDT benchmarking
#
# Need a deque as big as NB_INSTANCES if CORE_SCALING is 1
export CUSTOM_BOUNDS="NB_INSTANCES=2500000"
# Timings: 'crash #674' 1.3 0.7
# SCRIPT_ROOT=./scripts ./scripts/perfDriver.sh edtCreateStickySync

SCRIPT_ROOT=./scripts ./scripts/perfDriver.sh edtCreateFinishSync
SCRIPT_ROOT=./scripts ./scripts/perfDriver.sh edtCreateLatchSync

export CUSTOM_BOUNDS="NB_INSTANCES=1 NB_ITERS=16000000" # 1.5s
SCRIPT_ROOT=./scripts ./scripts/perfDriver.sh edtTemplate0Create
SCRIPT_ROOT=./scripts ./scripts/perfDriver.sh edtTemplate0Destroy

#
# Event benchmarking
#
# Event Creation
export CUSTOM_BOUNDS="NB_INSTANCES=1 NB_ITERS=10000000" # 1.3s
SCRIPT_ROOT=./scripts ./scripts/perfDriver.sh event0StickyCreate
SCRIPT_ROOT=./scripts ./scripts/perfDriver.sh event0OnceCreate
SCRIPT_ROOT=./scripts ./scripts/perfDriver.sh event0LatchCreate

export CUSTOM_BOUNDS="NB_INSTANCES=1 NB_ITERS=10000000" # 1s
SCRIPT_ROOT=./scripts ./scripts/perfDriver.sh event0StickyDestroy
SCRIPT_ROOT=./scripts ./scripts/perfDriver.sh event0OnceDestroy
SCRIPT_ROOT=./scripts ./scripts/perfDriver.sh event0LatchDestroy

# Event -> EDT | Time AddDep and Satisfy
export CUSTOM_BOUNDS="NB_ITERS=3000000 FAN_OUT=4" # 2.6
SCRIPT_ROOT=./scripts ./scripts/perfDriver.sh event1StickyFanOutEdtAddDep
SCRIPT_ROOT=./scripts ./scripts/perfDriver.sh event1OnceFanOutEdtAddDep

export CUSTOM_BOUNDS="NB_ITERS=4000000 FAN_OUT=4" # 2.7s
SCRIPT_ROOT=./scripts ./scripts/perfDriver.sh event1StickyFanOutEdtSatisfy
SCRIPT_ROOT=./scripts ./scripts/perfDriver.sh event1OnceFanOutEdtSatisfy

# Event -> Event | Time AddDep and Satisfy
export CUSTOM_BOUNDS="NB_ITERS=3000000 FAN_OUT=4" # 1.2
SCRIPT_ROOT=./scripts ./scripts/perfDriver.sh event2StickyFanOutStickyAddDep
SCRIPT_ROOT=./scripts ./scripts/perfDriver.sh event2OnceFanOutOnceAddDep
SCRIPT_ROOT=./scripts ./scripts/perfDriver.sh event2LatchFanOutLatchAddDep

export CUSTOM_BOUNDS="NB_ITERS=4000000 FAN_OUT=4" # 1s
SCRIPT_ROOT=./scripts ./scripts/perfDriver.sh event2StickyFanOutStickySatisfy
SCRIPT_ROOT=./scripts ./scripts/perfDriver.sh event2LatchFanOutLatchSatisfy
export CUSTOM_BOUNDS="NB_ITERS=3000000 FAN_OUT=4" # 1.3s
SCRIPT_ROOT=./scripts ./scripts/perfDriver.sh event2OnceFanOutOnceSatisfy

#
# Pretty printing results into a digest report
#

function extractThroughput() {
    local file=$1
    local  __resultvar=$2
    res=`more ${file} | grep "^1" | sed -e "s/  / /g" | cut -d' ' -f2-2`
    eval $__resultvar="'$res'"
}

# Returns individual operation duration in nanoseconds
function getTimePerOpFromThroughput() {
    local file=$1
    local  __resultvar=$2
    tput=
    extractThroughput $file tput
    scale=9
    res=`echo "scale=$scale; (10^9 / $tput)" | bc`
    eval $__resultvar="'$res'"
}

function printTimePerOpNano() {
    local file=$1
    local outfile=$2
    local text=$3
    opDuration=
    getTimePerOpFromThroughput $file opDuration
    echo "$text $opDuration (ns)" >> ${outfile}
}

#
# Nice printing
#

outfile="$PWD/digest-report"

echo "Average OCR operations cost for x86 with single OCR worker" > ${outfile}
echo "" >> ${outfile}

printTimePerOpNano report-edtCreateLatchSync ${outfile} '* An EDT creation takes:'
printTimePerOpNano report-edtCreateFinishSync ${outfile} '* An EDT creation inside a finish EDT takes:'
printTimePerOpNano report-edtTemplate0Create ${outfile} '* An EDT template creation takes:'
printTimePerOpNano report-edtTemplate0Destroy ${outfile} '* An EDT template destruction takes:'

printTimePerOpNano report-event0StickyCreate ${outfile} '* A sticky event creation takes:'
printTimePerOpNano report-event0OnceCreate ${outfile} '* A once event creation takes:'
printTimePerOpNano report-event0LatchCreate ${outfile} '* A latch event creation takes:'

printTimePerOpNano report-event0StickyDestroy ${outfile} '* A sticky event destruction takes:'
printTimePerOpNano report-event0OnceDestroy ${outfile} '* A once event destruction takes:'
printTimePerOpNano report-event0LatchDestroy ${outfile} '* A latch event destruction takes:'

FAN_OUT=4
printTimePerOpNano report-event1StickyFanOutEdtAddDep ${outfile} "* Add dependences between a source sticky event to ${FAN_OUT} EDT's slots takes:"
printTimePerOpNano report-event1OnceFanOutEdtAddDep ${outfile} " *Add dependences between a source once event to ${FAN_OUT} EDT's slots takes:"

printTimePerOpNano report-event1StickyFanOutEdtSatisfy ${outfile} "* Satisfy a sticky event that has ${FAN_OUT} EDT as dependences takes:"
printTimePerOpNano report-event1OnceFanOutEdtSatisfy ${outfile} "* Satisfy a once event that has ${FAN_OUT} EDT as dependences takes:"

printTimePerOpNano report-event2StickyFanOutStickyAddDep ${outfile} "* Add dependences between a source sticky event to ${FAN_OUT} sticky events takes:"
printTimePerOpNano report-event2OnceFanOutOnceAddDep ${outfile} "* Add dependences between a source once event to ${FAN_OUT} once events takes:"
printTimePerOpNano report-event2LatchFanOutLatchAddDep ${outfile} "* Add dependences between a source latch event to ${FAN_OUT} latch events takes:"

printTimePerOpNano report-event2StickyFanOutStickySatisfy ${outfile} "* Satisfy a sticky event that has ${FAN_OUT} sticky as dependences takes:"
printTimePerOpNano report-event2LatchFanOutLatchSatisfy ${outfile} "* Satisfy a latch event that has ${FAN_OUT} latch as dependences takes:"
printTimePerOpNano report-event2OnceFanOutOnceSatisfy ${outfile} "* Satisfy a once event that has ${FAN_OUT} once as dependences takes:"
