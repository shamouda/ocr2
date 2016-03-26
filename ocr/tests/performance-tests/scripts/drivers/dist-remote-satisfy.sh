#
# OCR Object micro-benchmarks X86 Single Node
#

if [[ -z "$SCRIPT_ROOT" ]]; then
    echo "SCRIPT_ROOT environment variable is not defined"
    exit 1
fi


unset OCR_CONFIG

. ${SCRIPT_ROOT}/drivers/utils.sh


if [[ -z "${OCRRUN_OPT_TPL_NODEFILE}" ]]; then
	if [[ -f "${PWD}/nodelist" ]]; then
		export OCRRUN_OPT_TPL_NODEFILE="$PWD/nodelist"
	fi
fi

export LOGDIR=`mktemp -d logs_dist-remote-satisfy.XXXXX`
export NAME_EXP="remoteSatisfy"
export OCR_TYPE=x86-mpi

#
# Experiment 1:
#   - 2-2 workers on X86-mpi
#   - Assertions VS none
#/
export NODE_SCALING="2"
export CORE_SCALING="2"

#
# Assertions on
# export EXT="ext"
unset NO_DEBUG
buildOcr

export NAME=remoteLatchSatisfy
export REPORT_FILENAME_EXT="-${OCR_TYPE}-assertOn"
runProg

#
# Assertions off
# export EXT="ext"
export NO_DEBUG=yes
buildOcr

export NAME=remoteLatchSatisfy
export REPORT_FILENAME_EXT="-${OCR_TYPE}-assertOff"
runProg


# Experiment 2:
#   - No assertions
#   - Process incoming message through EDT VS in-place


#
# -DCOMMWRK_PROCESS_SATISFY
export EXT="-cwProcess"
export NO_DEBUG=yes
export CFLAGS_USER="-DCOMMWRK_PROCESS_SATISFY"
# buildOcr

export NAME=remoteLatchSatisfy
export REPORT_FILENAME_EXT="-${OCR_TYPE}-assertOff${EXT}"
runProg


#
# Experiment 3: Worker Scaling
#   - Worker scaling: '2 -> N' both on sender and receiver end
#   - Sender: Still a single EDT
#   - Receiver: Up to (N-1) computation workers
#

export NODE_SCALING="2"
export CORE_SCALING="2 4 8 16"

export NAME=remoteLatchSatisfy
export REPORT_FILENAME_EXT="-${OCR_TYPE}-assertOff-scaling"
runProg

export EXT="-cwProcess"
unset CFLAGS_USER
export NO_DEBUG=yes
buildOcr

export NAME=remoteLatchSatisfy
export REPORT_FILENAME_EXT="-${OCR_TYPE}-assertOff-scaling${EXT}"
runProg
