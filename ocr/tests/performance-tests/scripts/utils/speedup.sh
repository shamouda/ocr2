#
# Input: a string of values
# Output: speedup

VALUES="$1"

# Check arg validity

if [[ "$VALUES" == "" ]]; then
    echo "error: ${0} no data points"
    exit 1
fi

# Computing Speed-up
SCALE=3
base=`echo $VALUES | cut -d' ' -f1-1`
acc=""
for datum in `echo $VALUES`; do
    res=`echo "scale=$SCALE; ($datum / $base)" | bc`
    if [[ -z "$acc" ]]; then
        acc="$res"
    else
        acc="$acc $res"
    fi
done

echo "$acc"
