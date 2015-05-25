#
# Extract and prints unique values for 'workload' data
# from a log file to standard output formatted as one
# workload unit per line
#
# Arguments:
#   FILE:               The file to be parsed
#
# Expected input format:
# "Workload    (unit): 7"
# "Workload    (unit): 7"
# "Workload    (unit): 9"
# "Workload    (unit): 9"

# File to parse
FILE=$1

# Assumes there are a number of consecutive workload values
DATA=`grep Workload $FILE | cut -d' ' -f 6-6 | uniq `

echo $DATA | tr ' ' '\n'
