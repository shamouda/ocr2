#!/bin/bash

#
# Extract columns from a set of files and paste them together into a single file
# All files must be formatted identically.

# Arguments:
#   OUTFILE:            File to write the output to
#   FILES:              A list of files to extract the columns from

OUTFILE=$1
FILES="${@:2}"

# Geared for throughput extraction
RESULT_COL=2
IGNORE_TOP_LINES=5 #TODO this should become a parameter in a config file, it's spread all voer the place
TMPDIR=`mktemp -d -p ${SCRIPT_ROOT} tmpdir.XXXXXX`

rm -f ${OUTFILE} 2>/dev/null
touch ${OUTFILE}

${SCRIPT_ROOT}/extractors/extractReportColDataPoint.sh ${RESULT_COL} ${IGNORE_TOP_LINES} ${TMPDIR} ${OUTFILE} "${FILES}"

rm -Rf ${TMPDIR}
