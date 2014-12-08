import sys
import os
import numpy

from operator import itemgetter

SRC_INDEX = 7
DEST_INDEX = 9
SIZE_INDEX = 11
MAR_TIME_INDEX = 13
SEND_TIME_INDEX = 15
RCV_TIME_INDEX = 17
UMAR_TIME_INDEX = 19
TYPE_INDEX = 21

def showUniqSizes(log):
    sizes = []
    for line in log:
        sizes.append(line.split()[SIZE_INDEX])

    print set(sizes)

def strip(dbgLog):
    os.system("grep Unmarshalled " + str(dbgLog) + " > filtered.txt")

def netTime(msgs):
    excelData = open('timeData.csv', 'a')
    averages = open('averageTimes.txt', 'a')

    #Arrange file to properly open in excel and add heading
    excelData.write('sep=,\n')
    excelData.write(' ,PreNetwork,overNetwork,postNetwork\n')

    preNetTotal = []
    overNetTotal = []
    postNetTotal = []
    endToEndTotal = []

    for i in xrange(len(msgs)):

        if len(msgs[i].split()) != 22:
            continue

        #Begin string to write comma-seperated line to file.
        csvString = msgs[i].split()[SIZE_INDEX]

        #Pre network latency
        preNetMsgTime = (int(msgs[i].split()[SEND_TIME_INDEX]) - int(msgs[i].split()[MAR_TIME_INDEX]))/float(1000)
        preNetTotal.append(preNetMsgTime)
        csvString += ',' + str(preNetMsgTime)

        #Over network latency
        overNetMsgTime = (int(msgs[i].split()[RCV_TIME_INDEX]) - int(msgs[i].split()[SEND_TIME_INDEX]))/float(1000)
        overNetTotal.append(overNetMsgTime)
        csvString += ',' + str(overNetMsgTime)

        #Post network latency
        postNetMsgTime = (int(msgs[i].split()[UMAR_TIME_INDEX]) - int(msgs[i].split()[RCV_TIME_INDEX]))/float(1000)
        postNetTotal.append(postNetMsgTime)
        csvString += ',' + str(postNetMsgTime)

        #End to end latency
        endToEndMsgTime = (int(msgs[i].split()[UMAR_TIME_INDEX]) - int(msgs[i].split()[MAR_TIME_INDEX]))/float(1000)
        endToEndTotal.append(endToEndMsgTime)

        excelData.write(csvString + ',' + msgs[i].split()[TYPE_INDEX] + '\n')

    #Calculate averages
    preAve = sum(preNetTotal)/float(len(preNetTotal))
    netAve = sum(overNetTotal)/float(len(overNetTotal))
    postAve = sum(postNetTotal)/float(len(postNetTotal))
    endToEndAve = sum(endToEndTotal)/float(len(endToEndTotal))

    #Calculate standard deviations
    preStdev = numpy.std(preNetTotal, axis=0)
    netStdev = numpy.std(overNetTotal, axis=0)
    postStdev = numpy.std(postNetTotal, axis=0)
    endToEndStdev = numpy.std(endToEndTotal, axis=0)

    #Calculate percentages
    prePerc = "%.2f" % ((sum(preNetTotal)/float(sum(endToEndTotal)))*100)
    overNetPerc = "%.2f" % ((sum(overNetTotal)/float(sum(endToEndTotal)))*100)
    postPerc = "%.2f" % ((sum(postNetTotal)/float(sum(endToEndTotal)))*100)

    #Write summary
    averages.write('Total Messages Observed: ' + str(len(msgs)) + '\n\n')
    averages.write('Pre network latency average (marshalling to MPI send): ' + str(preAve) + ' microseconds\n')
    averages.write('Pre network latency standard deviation: ' + str(preStdev) + ' microseconds\n')
    averages.write('Percentage: ' + str(prePerc) + '%\n\n')
    averages.write('Over network latency average (MPI send to MPI receive): ' + str(netAve) + ' microseconds\n')
    averages.write('Over network latency standard deviation: ' + str(netStdev) + ' microseconds\n')
    averages.write('Percentage: ' + str(overNetPerc) + '%\n\n')
    averages.write('Post network latency average (MPI receive to unmarshalling): ' + str(postAve) + ' microseconds\n')
    averages.write('Post network latency standard deviation: ' + str(postStdev) + ' microseconds\n')
    averages.write('Percentage: ' + str(postPerc) + '%\n\n')
    averages.write('Overall end to end latency average: ' + str(endToEndAve) + ' microseconds\n')
    averages.write('Overall end to end latency standard deviation: ' + str(endToEndStdev) + ' microseconds\n')

    excelData.close()
    averages.close()

#==================== MAIN =========================
def main():

    #Arrange Log Data
    if len(sys.argv) != 2:
        print "Error! Correct usage: python times.py <filename>"
        sys.exit(0)
    dbgLog = sys.argv[1]
    strip(dbgLog)
    fp = open('filtered.txt', 'r')

    #Process Data
    allMsgs = fp.readlines()
    netTime(allMsgs)

    #showUniqSizes(allMsgs)

    #cleanup
    fp.close()
    os.remove('filtered.txt')

main()
