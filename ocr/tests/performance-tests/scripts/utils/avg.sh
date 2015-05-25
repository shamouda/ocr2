#
# Input: a string of values
# Output: avg count

VALUES="$1"
# significant digits
SCALE=3

acc=0
count=0

for datum in `echo $VALUES`; do
    acc=`echo "$acc + $datum" | bc`
    let count=${count}+1
done

avg=0;
if [[ $count -gt 0 ]]; then
    avg=`echo "scale=$SCALE; $acc / $count" | bc `
fi

echo "$avg $count"

