
#Takes two input files containing one entry per line and compare one to one.

REPORT1=$1
REPORT2=$2

COL1=`more $REPORT1 | tr '\n' ' '`
COL2=`more $REPORT2 | tr '\n' ' '`

IFS=' ' read -r -a ARRAY1 <<< "$COL1"
IFS=' ' read -r -a ARRAY2 <<< "$COL2"

let lg=${#ARRAY1[@]}
let i=0;
while (( i < lg )); do
    echo "scale=3; (${ARRAY1[$i]} / ${ARRAY2[$i]})" | bc
    let i=$i+1
done
