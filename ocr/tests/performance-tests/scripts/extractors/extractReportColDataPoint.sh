#!/bin/bash

#
# Extract columns from a set of files and paste them together into a single file
# All files must be formatted identically.
#
# Arguments:
#   RESULT_COL:         The column number to extract (1-based)
#   IGNORE_TOP_LINES:   Number of top lines to ignore from input files
#   TMPDIR              Temporary folder for intermediate files
#   OUTFILE:            File to write the output to
#   FILES:              A list of files to extract the columns from

RESULT_COL=$1
IGNORE_TOP_LINES=$2
TMPDIR=$3
OUTFILE=$4
FILES="${@:5}"

rm -f ${OUTFILE} 2>/dev/null
touch ${OUTFILE}
for file in `echo "${FILES}"`; do
    tail -n+${IGNORE_TOP_LINES} $file | tr -s ' ' | cut -d' ' -f${RESULT_COL}-${RESULT_COL} > ${TMPDIR}/aggcut1.tmp
    paste -d ' ' ${OUTFILE} ${TMPDIR}/aggcut1.tmp > ${TMPDIR}/aggcut2.tmp
    mv ${TMPDIR}/aggcut2.tmp ${OUTFILE}
done

rm -f ${TMPDIR}/aggcut1.tmp 2>/dev/null
