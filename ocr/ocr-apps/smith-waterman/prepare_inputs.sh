#!/bin/bash

if [ $# -ne 5 ]
then
    echo "Usage: ${0} <tile width> <tile height> <string1 file> <string2 file> <score file>"
    exit 1
fi

# Determine the size of first file
SIZE1=`stat -c %s ${3}`
SIZE2=`stat -c %s ${4}`
SIZE3=`stat -c %s ${5}`
echo $SIZE1 $SIZE2 $SIZE3

# Concatenate both into a single file
cat ${3} ${4} ${5} > smithwaterman_output.bin
