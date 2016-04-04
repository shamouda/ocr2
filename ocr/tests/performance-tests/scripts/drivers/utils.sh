#
# Bunch of utility functions for drivers scripts
#


#
# Build OCR and run programs
#
function buildOcr() {
    SAVE=$PWD
    export RES=0
    if [[ -z "${OCR_NOCLEAN}" ]]; then
        cd ../..; make squeaky; cd -
    fi
    cd ../../build/${OCR_TYPE}; make && make install; RES=$?; cd -
    if [[ $RES -ne 0 ]]; then
        exit 1
    fi
    cd $SAVE
}

function runProg() {
    local OPTS=""
    if [[ -n "${LOGDIR}" ]]; then
        echo "GENERATING LOG DIR OPTIONS"
        OPTS+="-logdir ${LOGDIR}"
    fi
    rm -Rf build/*
    ./scripts/perfDriver.sh ${OPTS} -target ${OCR_TYPE} ${NAME}
}


#
# Pretty printing results into a digest report
#
function extractSingleThroughput() {
    local file=$1
    local  __resultvar=$2
    res=`more ${file} | grep "^[0-9]" | sed -e "s/  / /g" | cut -d' ' -f2-2`
    lines=`echo "$res" | wc -l`
    if [[ ${lines} -eq 1 ]]; then
        eval $__resultvar="'$res'"
    else
        echo ""
        echo "error: extractSingleThroughput does not handle multiple throughput entries in report file"
        echo ""
        exit 1
    fi
}

# Returns individual operation duration in nanoseconds
function getTimePerOpFromThroughput() {
    local file=$1
    local  __resultvar=$2
    tput=
    extractSingleThroughput $file tput
    scale=9
    res=`echo "scale=$scale; (10^9 / $tput)" | bc`
    eval $__resultvar="'$res'"
}

function printTimePerOpNano() {
    local file=$1
    local outfile=$2
    local raw_outfile=$3
    local text=$4
    if [[ -e $file ]]; then
        opDuration=
        getTimePerOpFromThroughput $file opDuration
        echo "$text $opDuration (ns)" >> ${outfile}
        if [[ -n "$raw_outfile" ]]; then
            echo "$opDuration" >> ${raw_outfile}
        fi
    fi
}
