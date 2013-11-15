FREQ=1 # How often should this be updated - in seconds

# Create the gnuplot script

echo "set xlabel 'Worker number'" > plot.plt
echo "set ylabel '#Nodes'" >> plot.plt
echo "set y2label 'Cluster size'" >> plot.plt
echo "set y2tics" >> plot.plt
echo "unset ytics" >> plot.plt
echo "set ytics nomirror" >> plot.plt
echo "set style fill solid 1.0 noborder" >> plot.plt
echo "set style line 1 lt 2 lw 2" >> plot.plt
echo "plot '1.txt' using 1:2 with boxes notitle, '1.txt' using 1:3 with impulses ls 1 notitle axes x1y2" >> plot.plt
echo "pause ${FREQ}" >> plot.plt
echo "reread;" >> plot.plt

# Start gnuplot
gnuplot plot.plt &

# Make up data; to be replaced by OCR output

while [ true ];
do
cp 2.txt 1.txt
sleep ${FREQ}
cp 3.txt 1.txt
sleep ${FREQ}
done   
