import sys
import re
import subprocess
import os

def stripMpiSends(dbgLog):
    os.system("grep \'posting isend\' " + str(dbgLog) + " > filtered.txt")

def getUniqueTypes(logFile):
    splitLog = []
    types = []

    for line in logFile:
        splitLog.append(line.split())

    for line in splitLog:
        types.append(line[14])
    return set(types)

def getUniqueWorkers(logFile):
    splitLog = []
    wrkrs = []

    for line in logFile:
        splitLog.append(line.split())

    for line in splitLog:
        wrkrs.append(line[2][2:])
    return set(wrkrs)

def analyzeOverhead(outFile, uniqTypes, logRecords, typeRecords):
    #Temporary file formatted to easily copy/paste to excel
    #excelFile = open('excel.txt', 'a')
    sumBytes = 0
    largestdb = 0

    for i in logRecords:
        sumBytes += int(i.split()[16], 16)
        if int(i.split()[16], 16) > largestdb:
            largestdb = int(i.split()[16], 16)
    outFile.write("Problem size: " + str(bToMb(largestdb)) + "MB\n\n")
    #excelFile.write(str(bToMb(largestdb)) + "\n")
    for i in uniqTypes:
        msgName = ""
        totalCalls = 0
        totalBytes = 0
        for j in typeRecords:
            curType = str(i)
            if j.split()[1][2:] == curType:
                msgName = str(j.split()[0])
                break

        if msgName == "":
            #Should not get hit unless messageTypes.txt is not in sync with ocr-policy-domain.h
            print "Error!  Invalid message type."
            sys.exit(0)

        for rec in logRecords:
            curType = rec.split()[14]
            #check for PD equality (sanity)
            if rec.split()[5][:-1] == rec.split()[19]:
                print "Non-remote\n"
            if curType == i:
                totalCalls += 1
                totalBytes += int(rec.split()[16], 16)
        #TODO: write Gb for now, later implement conditional
        #conversion and write fitting value based on total bytes
        mb = bToMb(totalBytes)

        #gb = bToGb(totalBytes)
        #tb = bToTb(totalBytes)
        temp = (totalBytes/float(sumBytes))*100.
        perc = float("{:.4f}".format(temp))
        #excelFile.write(str(mb) + "," + str(perc) + "\n")
        outFile.write("============ " + str(msgName) + " ===========\n\n")
        outFile.write("Total MPI sends: " + str(totalCalls) + "\n")
        outFile.write("Megabytes sent: " + str(mb) + "\n")
        outFile.write("Percentage of total Bytes sent: " + str(perc) + "%\n\n\n")
def cleanup(dbgLog):
    #os.remove(dbgLog)
    os.remove('filtered.txt')


def bToGb(totalBytes):
    f = float((((totalBytes/1000)/1000)/1000.))
    gb = float("{:.4f}".format(f))
    return gb

def bToMb(totalBytes):
    f = float(((totalBytes/1000)/1000.))
    gb = float("{:.4f}".format(f))
    return gb

def bToTb(totalBytes):
    f = float(((((totalBytes/1000)/1000)/1000)/1000.))
    gb = float("{:.4f}".format(f))
    return gb

def main():

    #Show usage
    if len(sys.argv) != 2:
        print "Error.....  Usage: python timeline.py <inputFile>"
        sys.exit(0)
    dbgLog = sys.argv[1]

    #Grab only mpi sends from debug log
    stripMpiSends(dbgLog)

    #Open files and arrange data for analysis
    inFile = open('filtered.txt', 'r')
    logRecords = inFile.readlines()
    wrkrs = getUniqueWorkers(logRecords)
    uniqMsgTypes = getUniqueTypes(logRecords)
    outFile = open('analysis.txt', 'a')
    allTypes = open('messageTypes.txt', 'r')
    typeRecords = allTypes.readlines()

    #Run analysis and produce output files
    analyzeOverhead(outFile, uniqMsgTypes, logRecords, typeRecords)

    #Cleanup
    inFile.close()
    outFile.close()
    allTypes.close()
    cleanup(dbgLog)
    sys.exit(0)

main()
