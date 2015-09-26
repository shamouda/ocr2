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