FILE=$1
CR=$2

DATA=`grep Throughput $FILE | cut -d' ' -f 4-4`

#
# Output DATA as rows with 'CR' elements
#
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
