#
# Extract and prints 'Throughput' data from a log file
# to standard output formatted as rows of 'CR' elements
#
# Arguments:
#   FILE:               The file to be parsed
#   CR:                 Number of element per rows
#
# Expected input format:
# Throughput  (op/s): 1191.414997
#

FILE=$1
CR=$2

DATA=`grep Throughput $FILE | cut -d' ' -f 4-4`

COL_DATA=""
let i=0
let ub=${CR}-1
for elem in `echo $DATA`; do
    COL_DATA+="$elem "
    if [[ $i == ${ub} ]]; then
        echo "$COL_DATA"
        COL_DATA=""
        let i=0
    else
        let i=$i+1
    fi
done
