FREQ=1 # How often should this be updated - in seconds

# Create the gnuplot script

echo "set xlabel 'Time (s)'" > plot.plt
echo "set ylabel 'Throughput (#nodes processed)'" >> plot.plt
echo "set y2label 'Cluster size'" >> plot.plt
echo "set y2tics" >> plot.plt
echo "unset ytics" >> plot.plt
echo "set y2range[0:10]" >> plot.plt
echo "set ytics nomirror" >> plot.plt
echo "plot '1file.txt' using 1:2 w lines title 'Worker 1', '2file.txt' using 1:2 w lines title 'Worker 2', '3file.txt' using 1:2 w lines title 'Worker 3', '4file.txt' using 1:2 w lines title 'Worker 4', '0file.txt' using 1:2 w lines axes x1y2 title 'chunkSize'" >> plot.plt
echo "pause ${FREQ}" >> plot.plt
echo "reread;" >> plot.plt

# Start gnuplot
gnuplot plot.plt &

# Make up data; to be replaced by OCR output

while [ true ];
do
sleep ${FREQ}
done   
