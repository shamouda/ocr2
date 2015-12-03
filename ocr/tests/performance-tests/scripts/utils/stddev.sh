#
# Input arguments:
# 	- a string of values
# Output: avg stddev count
#

if [[ -z "$SCRIPT_ROOT" ]]; then
    echo "SCRIPT_ROOT environment variable is not defined"
    exit 1
fi

VALUES="$1"
PRINT_COUNT="$2"

# Check arg validity

if [[ "$VALUES" == "" ]]; then
    echo "error: ${0} no data points"
    exit 1
fi

# Check environment for utils

SCALE=${SCALE-3}

# Invoking average

avg_res=(`${SCRIPT_ROOT}/utils/avg.sh "$VALUES"`)

avg=${avg_res[0]}
count=${avg_res[1]}

# Computing stddev

diffs=0

for datum in `echo $VALUES`; do
    diffs=`echo "(($datum - $avg)^2)+$diffs" | bc`
done
stddev=`echo "scale=$SCALE; sqrt(($diffs / $count))" | bc`

echo "$avg ${stddev} ${count}"
