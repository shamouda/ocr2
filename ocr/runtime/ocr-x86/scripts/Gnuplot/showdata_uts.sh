#!/bin/bash

FREQ=1 # How often should this be updated - in seconds
FILE=utsOutput

rm $FILE

if [[ ! -a "$FILE" ]]; then
    echo "utsOutput not found"
fi

while [[ ! -a "$FILE" ]]; do
    sleep 1
done

#Give some time to the application to start writing
sleep 2

# Create the gnuplot script

echo "set xlabel 'Worker Number'" > plot.plt
echo "set ylabel 'Chunk size'" >> plot.plt
echo "set y2label 'Processed Nodes'" >> plot.plt
echo "set y2tics" >> plot.plt
echo "unset ytics" >> plot.plt
echo "set yrange [0:10]" >> plot.plt
echo "set ytics nomirror" >> plot.plt
echo "set style fill solid 1.0 noborder" >> plot.plt
echo "set style line 1 lt 2 lw 2" >> plot.plt
echo "plot 'utsOutput' using 1:2 with boxes notitle, 'utsOutput' using 1:3 with impulses ls 1 notitle axes x1y2" >> plot.plt
echo "pause ${FREQ}" >> plot.plt
echo "reread;" >> plot.plt

# Start gnuplot
gnuplot plot.plt &

# Make up data; to be replaced by OCR output

while [ true ];
do
#cp 2.txt 1.txt
sleep ${FREQ}
#cp 3.txt 1.txt
#sleep ${FREQ}
done   
