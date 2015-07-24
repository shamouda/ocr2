#
# This file is subject to the license agreement located in the file
# LICENSE and cannot be distributed without it.  This notice
# cannot be removed or modified
#


import sys
import re
from subprocess import call
import os
import time
import glob
import math
from operator import itemgetter


#Line split indices for each line from logs
START_TIME_INDEX = 9
END_TIME_INDEX = 11
FCT_NAME_INDEX = 7
WORKER_ID_INDEX = 2
PD_ID_INDEX = 1
GUID_INDEX = 5

#Constant flags for differentiating single/multi node runs
SINGLE_PD = 0
DIST_FIRST_PD = 1
DIST_MID_PD = 2
DIST_LAST_PD = 3

NEGLIGABLE_TIME_CUTOFF = 25000 #Ns

#JS dependence hosted on xstack webserver
JAVASCRIPT_URL = "http://xstack.exascale-tech.com/public/vis/dist/vis.js"
#CSS dependence hosted on xstack webserver
CSS_URL = "http://xstack.exascale-tech.com/public/vis/dist/vis.css"
#For min value helper functions
HUGE_CONSTANT = sys.maxint
#Naming constants for output files
STRIPPED_RECORDS = 'filtered.txt'
HTML_FILE_NAME = 'timeline'

max_edts_per_page = 2000
user_flag_sys     = False
user_flag_combine = False
user_flag_force_single = False

#========= Write EDT EXECTUION events to html file ==========
def postProcessData(inString, outFile, offSet, lastLineFlag, counter, color):
    words = inString.split()
    workerID = words[WORKER_ID_INDEX][WORKER_ID_INDEX:]
    fctName = words[FCT_NAME_INDEX]
    startTime = (int(words[START_TIME_INDEX]) - offSet)
    endTime = (int(words[END_TIME_INDEX]) - offSet)
    totalTime = "{0:.2f}".format(endTime - startTime)
    whichNode = words[PD_ID_INDEX][1:]
    curGuid = words[GUID_INDEX]
    uniqIDs = []


    doPrint = True
    if fctName == '&processRequestEdt' and user_flag_sys == False:
        doPrint = False

    if lastLineFlag == True:

        if doPrint  == True:
            outFile.write('\t\t{id: ' + str(counter) + ', group: ' + '\'' + str(workerID) + '\'' + ', title: \'EDT Name: ' + str(fctName) + '\\nExecution Time: ' + str(totalTime) + 'Ns\\nGuid: ' + str(curGuid) + '\', content: \'' + str(fctName) + '\', start: new Date(' + str(startTime) + '), end: new Date(' +  str(endTime) + '), style: "background-color: ' + str(color) + ';"}\n')
        outFile.write('\t]);\n\n')
    else:
        if doPrint == True:
            outFile.write('\t\t{id: ' + str(counter) + ', group: ' + '\'' + str(workerID) + '\'' + ', title: \'EDT Name: ' + str(fctName) + '\\nExecution Time: ' + str(totalTime) + 'Ns\\nGuid: ' + str(curGuid) + '\', content: \'' + str(fctName) + '\', start: new Date(' + str(startTime) + '), end: new Date(' +  str(endTime) + '), style: "background-color: ' + str(color) + ';"},\n')

#========== Strip Un-needed events from OCR debug log ========
def runShellStripDist(dbgLog):
    os.system("egrep -w \'EDT\\(INFO\\)|EVT\\(INFO\\)\' " + str(dbgLog) + " | egrep -w \'FctName\' > " + STRIPPED_RECORDS)
    return 0

#========== Strip Un-needed events from OCR debug log ========
def runShellStrip(dbgLog):
    os.system("egrep -w \'EDT\\(INFO\\)|EVT\\(INFO\\)\' " + str(dbgLog) + " | egrep -w \'FctName\' | grep -v '&processRequestEdt' > " + STRIPPED_RECORDS)
    return 0


#========= Strip PD specific records for distributed timelines ======
def createFilePerPD(pds, dbgLog):
    for pd in pds:
        pdStr = str(pd)
        os.system("egrep -w \'EDT\\(INFO\\)|EVT\\(INFO\\)\' " + str(dbgLog) + " | egrep -w \'\\[PD\\:" + pdStr + "\' > " + pdStr)


#========== Write Common open HTML tags to output file ==========
def appendPreHTML(outFile, pre):
    for line in pre:
        outFile.write(line)

    fp = open(STRIPPED_RECORDS, 'r')
    lines = fp.readlines()
    sortedLines = sortAscendingStartTime(lines)
    exeTime = getExecutionTime(sortedLines)
    totalEDTs = len(sortedLines)

    outFile.write('<p>Total (CPU) execution time: '+str(exeTime)+' nanoseconds</p>\n')
    outFile.write('<p>Total number of executed tasks: '+str(totalEDTs)+'</p>\n')

    outFile.write("<p><b>&nbsp;&nbsp;Workers</b></p>\n")
    outFile.write("<div id=\"visualization\"></div>\n")
    outFile.write("<script>\n")
    outFile.write("\tvar groups = new vis.DataSet([\n")


#========= Write initial values for Dist-x86 run ===========
def appendPreDistHTML(outFile, pre, pdNum, data, numSys):
    for line in pre:
        outFile.write(line)
    fp = open(STRIPPED_RECORDS, 'r')
    lines = fp.readlines()
    sortedLines = sortAscendingStartTime(lines)
    exeTime = getExecutionTime(sortedLines)
    totalEDTs = len(sortedLines)

    outFile.write('<p>Total (CPU) execution time: '+str(exeTime)+' nanoseconds</p>\n')
    outFile.write('<p>Total number of executed tasks: '+str(totalEDTs)+'</p>\n')

    numUsr = getNumUsrEDTs(data)

    outFile.write('<hr>')
    outFile.write('<p><b>Policy Domain ID: '+str(pdNum-1)+'</b></p>\n')
    outFile.write("<p>Num user EDTs executed: "+str(numUsr)+"</p>\n")
    outFile.write("<p>Num distributed system generated EDTs executed: "+str(numSys)+"</p>\n")
    outFile.write('<hr>')
    outFile.write("<p><b>&nbsp;&nbsp;Workers</b></p>\n")
    outFile.write("<div id=\"visualization"+str(pdNum)+"\"></div>\n")
    outFile.write("<script>\n")
    outFile.write("\tvar groups = new vis.DataSet([\n")

#========== Write HTML Data for worker (thread) groups ==========
def appendWorkerIdHTML(outFile, uniqueWorkers):
    uniqWrkrs = list(uniqueWorkers)
    for i in xrange(1, len(uniqWrkrs)+1):
        curID = uniqWrkrs[i-1]
        if i == len(uniqWrkrs):
            outFile.write('\t\t{id: ' + '\'' + str(curID) + '\'' + ', content: \'' + str(curID) + '\', value: ' + str(i) + '}\n')
            outFile.write('\t]);\n\n')
            break
        else:
            outFile.write('\t\t{id: ' +  '\'' + str(curID) + '\'' + ', content: \'' + str(curID) + '\', value: ' + str(i) + '},\n')

    outFile.write('\tvar items = new vis.DataSet([\n')

#========== Write HTML Data for any PD value that is not first, or last ==========
def appendIntermediateHTML(outFile, flag, pageNum, start, end, timeOffset, numThreads, pflag, pdNum):
    modStart = math.ceil((int(start) - timeOffset))
    modEnd = math.ceil((int(end) - timeOffset))
    outFile.write('\tvar container = document.getElementById(\'visualization'+str(pdNum)+'\');\n')
    outFile.write('\tvar options = {\n')
    outFile.write('\t\tgroupOrder: function (a, b) {\n')
    outFile.write('\t\t\treturn a.value - b.value;\n')
    outFile.write('\t\t},\n')
    outFile.write('\t\tstack: true,\n')
    outFile.write('\t\tstart: new Date(' + str(modStart) + '),\n')
    outFile.write('\t\tmin: new Date(' + str(modStart) + '),\n')
    outFile.write('\t\tmax: new Date(' + str(modEnd) + '),\n')
    outFile.write('\t\tzoomMax: ' + str(modEnd) + ',\n')
    outFile.write('\t\teditable: false,\n')
    outFile.write('\t\tshowMinorLabels: false,\n')
    outFile.write('\t\tshowMajorLabels: false\n')
    outFile.write('\t};\n\n')
    outFile.write('\tvar timeline = new vis.Timeline(container);\n')
    outFile.write('\ttimeline.setOptions(options);\n')
    outFile.write('\ttimeline.setGroups(groups);\n')
    outFile.write('\ttimeline.setItems(items);\n')
    outFile.write('</script>\n')
    if pflag != DIST_LAST_PD:
        outFile.write('<BR><BR>\n')
    else:
        #TODO: Call function to write nav buttons
        writeNavButtons(outFile, flag, pageNum, numThreads)
        outFile.write('</body>\n')
        outFile.write('</html>\n')


#========== Write Common closing HTML tags to output file ========
def appendPostHTML(outFile, flag, pageNum, start, end, timeOffset, numThreads):
    modStart = math.ceil((int(start) - timeOffset))
    modEnd = math.ceil((int(end) - timeOffset))

    outFile.write('\tvar container = document.getElementById(\'visualization\');\n')
    outFile.write('\tvar options = {\n')
    outFile.write('\t\tgroupOrder: function (a, b) {\n')
    outFile.write('\t\t\treturn a.value - b.value;\n')
    outFile.write('\t\t},\n')
    outFile.write('\t\tstack: false,\n')
    outFile.write('\t\tstart: new Date(' + str(modStart) + '),\n')
    outFile.write('\t\tmin: new Date(' + str(modStart) + '),\n')
    outFile.write('\t\tmax: new Date(' + str(modEnd) + '),\n')
    outFile.write('\t\tzoomMax: ' + str(modEnd) + ',\n')
    outFile.write('\t\teditable: false,\n')
    outFile.write('\t\tshowMinorLabels: false,\n')
    outFile.write('\t\tshowMajorLabels: false\n')
    outFile.write('\t};\n\n')
    outFile.write('\tvar timeline = new vis.Timeline(container);\n')
    outFile.write('\ttimeline.setOptions(options);\n')
    outFile.write('\ttimeline.setGroups(groups);\n')
    outFile.write('\ttimeline.setItems(items);\n')
    outFile.write('</script>\n')

    writeNavButtons(outFile, flag, pageNum, numThreads)

    outFile.write('</body>\n')
    outFile.write('</html>\n')

def writeNavButtons(outFile, pageFlag, pageNum, numThreads):
    nextPage = pageNum+1
    prevPage = pageNum-1
    if pageFlag == "first":
    #Write 'next' button only
        outFile.write('<div>\n')
        outFile.write('<BR>\n')
        outFile.write('\t<form action="./' + HTML_FILE_NAME + str(nextPage) + '.html">\n')
        outFile.write('\t\t<input type="submit" value="Next">\n')
        outFile.write('</div>\n')
        outFile.write('</form>\n')

    if pageFlag == "mid":
    #Write 'next' and 'previous' buttons
        outFile.write('<div>\n')
        outFile.write('<BR>\n')
        outFile.write('\t<form action="./' + HTML_FILE_NAME + str(nextPage) + '.html">\n')
        outFile.write('\t\t<input type="submit" value="Next">\n')
        outFile.write('</div>\n')
        outFile.write('</form>\n')


        outFile.write('<div>\n')
        outFile.write('<BR>\n')
        outFile.write('\t<form action="./'+ HTML_FILE_NAME + str(prevPage) + '.html">\n')
        outFile.write('\t\t<input type="submit" value="Previous">\n')
        outFile.write('</div>\n')
        outFile.write('</form>\n')

    if pageFlag == "last":
    #Write 'previous' button only
        outFile.write('<div>\n')
        outFile.write('<BR>\n')
        outFile.write('\t<form action="./'+ HTML_FILE_NAME + str(prevPage) + '.html">\n')
        outFile.write('\t\t<input type="submit" value="Previous">\n')
        outFile.write('</div>\n')
        outFile.write('</form>\n')


#======== Get set of unique PDs present in execution =========
def getNodes(logFile):
    fp = open(logFile, 'r')
    lines = fp.readlines()

    pds = []
    for line in lines:
        pds.append(line.split()[PD_ID_INDEX][4:])

    uniqPds = (set(pds))
    return uniqPds

#======== Compute and return total executin time ==========
def getExecutionTime(logFile):
    firstLine = logFile[0].split()
    lastLine = logFile[-1].split()
    offset = int(firstLine[START_TIME_INDEX])
    totalTime = (int(lastLine[END_TIME_INDEX]) - offset)
    return totalTime

#======== Gathers unique EDT names to be mapped to color ======
def getUniqueFctNames(logFile):
    splitLog = []
    names = []
    for line in logFile:
        splitLog.append(line.split())

    for element in splitLog:
        names.append(element[FCT_NAME_INDEX])

    uniqNames = (set(names))
    return uniqNames

#======== Get the number of unique threads =========
def getUniqueWorkers(logFile):
    splitLog = []
    workers = []
    for line in logFile:
        splitLog.append(line.split())

    for element in splitLog:
        workers.append(element[WORKER_ID_INDEX][WORKER_ID_INDEX:])

    uniqWorkers = (set(workers))
    return uniqWorkers

#=========Account for (possible) out of order log writes by sorting=====
def sortAscendingStartTime(logFile):
    splitLog = []
    sortedList = []
    for line in logFile:
        splitLog.append(line.split())

    for element in splitLog:
        element[START_TIME_INDEX] = int(element[START_TIME_INDEX])

    tempSorted = sorted(splitLog, key=itemgetter(START_TIME_INDEX))

    for element in tempSorted:
        element[START_TIME_INDEX] = str(element[START_TIME_INDEX])

    for element in tempSorted:
        sortedList.append((' '.join(element))+'\n')

    return sortedList

#========== Sorts by worker to keep timeline format constant ===========
def sortByWorker(sortedLog, uniqWorkers):
    sortedWrkrs = []
    for worker in uniqWorkers:
        for line in sortedLog:
            lineSplit = line.split()
            if str(lineSplit[WORKER_ID_INDEX][WORKER_ID_INDEX:]) == str(worker):
                sortedWrkrs.append(line)

    return sortedWrkrs

#========== Subdivide data to accomodate multiple pages ==============
def getSub(log, flag, pageNum):
    global max_edts_per_page

    if flag == "single":
        startIdx = 0
        endIdx = len(log)
    elif flag == "first":
        startIdx = 0
        endIdx = (pageNum*max_edts_per_page)
    elif flag == "mid":
        startIdx = ((pageNum-1)*max_edts_per_page)
        endIdx = ((pageNum)*max_edts_per_page)
    elif flag == "last":
        startIdx = ((pageNum-1)*max_edts_per_page)
        endIdx = len(log)

    sub = []
    for i in xrange(startIdx, endIdx):
        sub.append(log[i])

    return sub

#======== get color per Unique Function ========
def getColor(fctName, colorMap):
    for i in range(len(colorMap)):
        if colorMap[i][0] == fctName:
            return colorMap[i][1]
    print 'error, no color value.'
    sys.exit(0)

#======= key-val pair up: Functions to aesthetically pleasing colors =======
def assignColors(uniqFunctions):
    #TODO Expand this color list if need be.
    cssColors = ['Blue', 'Red', 'DarkSeaGreen', 'Coral', 'Yellow', 'DarkOrange', 'DarkOrchid', 'HotPink', 'Gold', 'PaleGreen', 'Tan', 'Thistle', 'Tomato', 'Cadet Blue', 'Salmon', 'MediumTurquoise', 'DeepPink', 'Indigo', 'Khaki', 'LightBlue', 'Maroon', 'LimeGreen', 'BurlyWood', 'Brown', 'Crimson', 'LawnGreen', 'LightCyan', 'MediumOrchid', 'Linen', 'LightCoral', 'MediumSpringGreen', 'LightSteelBlue', 'Blue', 'Red', 'DarkSeaGreen', 'Coral', 'Yellow', 'DarkOrange', 'DarkOrchid', 'HotPink', 'Gold', 'PaleGreen', 'Tan', 'Thistle', 'Tomato', 'Cadet Blue', 'Salmon', 'MediumTurquoise', 'DeepPink', 'Indigo', 'Khaki', 'LightBlue', 'Maroon', 'LimeGreen', 'BurlyWood', 'Brown', 'Crimson', 'LawnGreen', 'LightCyan', 'MediumOrchid', 'Linen', 'LightCoral', 'MediumSpringGreen', 'LightSteelBlue']
    fctList = list(uniqFunctions)
    tempColors = []
    for i in range(len(fctList)):
        tempColors.append(cssColors[i])

    mappedColors = zip(fctList, tempColors)
    return mappedColors

#======= Getter for Minumum start time among subset of EDTs ========
def getMinStart(data):
    minStart = HUGE_CONSTANT
    idx = 0
    for i in range(len(data)):
        curStart = int(data[i][0].split()[9])
        if curStart < minStart:
            minStart = curStart
            idx = i
    return [minStart, idx]

#======= Getter for greatest start time among subset of EDTs =======
def getMaxEnd(data):
    maxEnd = 0
    idx = 0
    for i in range(len(data)):
        curEnd = int(data[i][0].split()[11])
        if curEnd > maxEnd:
            maxEnd = curEnd
            idx = i
    return [maxEnd, idx]

#======= Differentiate system generated EDTs for Dist-x86 ========
def getNumSysEDTs(data):
    count = 0
    for i in range(len(data)):
        curName = str(data[i][0].split()[7])
        if curName == '&processRequestEdt':
            count += 1

    return count

#======= Differentiate User-code generated EDTs ========
def getNumUsrEDTs(data):
    count = 0
    for i in range(len(data)):
        curName = str(data[i][0].split()[7])
        if curName != '&processRequestEdt':
            count += 1

    return count

#======== Merge subsequent EDTs of duplicate type =========
def condenseDuplicates(data):

    #Arrange By Worker
    wrkrData = []
    for i in range(len(data[0])):
        wrkrData.append(data[0][i][0].split()[2][2:])

    wrkrList = list(set(wrkrData))
    splitSet = []
    for j in wrkrList:
        cur = []
        for i in range(len(data[0])):
            if data[0][i][0].split()[2][2:] == j:
                cur.append(data[0][i])
        splitSet.append(cur)

    for sub in splitSet:
        count = 0
        i = 0
        initLen = len(sub)
        numCond = 0
        while count < initLen-1:
            curEnd = int(sub[i][0].split()[END_TIME_INDEX])
            nextStart = int(sub[i+1][0].split()[START_TIME_INDEX])
            curType  = sub[i][0].split()[FCT_NAME_INDEX]
            nextType = sub[i+1][0].split()[FCT_NAME_INDEX]
            idleTime = nextStart - curEnd
            #Subsequent EDTs are them same and timei between, negligable.... condense them
            if (sub[i][0].split()[FCT_NAME_INDEX] == sub[i+1][0].split()[FCT_NAME_INDEX]) and ((idleTime > 0) and (idleTime < NEGLIGABLE_TIME_CUTOFF)):
                #Get finish time of next EDT
                numCond+=1
                nextFinishTime = sub[i+1][0].split()[11]

                splitString = sub[i][0].split()
                splitString[11] = nextFinishTime
                repStr = ''.join((str(ele)+ ' ') for ele in splitString)
                sub[i][0] = repStr

                #delete next item
                del sub[i+1]
                i-=1

            i+=1
            count+=1

    #create new dataSet
    inner = []
    uniqIDs = []
    newData = []
    for i in range(len(splitSet)):
        for j in range(len(splitSet[i])):
            if splitSet[i][j][2] not in uniqIDs:
                uniqIDs.append(splitSet[i][j][2])
                inner.append(splitSet[i][j])
    newData.append(inner)

    return newData

#======= Pre file-writing check to account for EDTs running beyond a single page =======
def preCheckEdtLengthAndWrite(toHtml, exeTime, numThreads, numEDTs, totalTime, uniqWorkers, timeOffset, flag, pdNum):

    global max_edts_per_page

    dataCopy = toHtml
    if user_flag_combine == True:
        #Combine back to back EDTs
        newData = []
        for i in range(len(toHtml)):
            tempList = []
            tempList.append(toHtml[i])
            newData.append(condenseDuplicates(tempList)[0])
        toHtml = newData

    #Check for EDT times extending past current page; rewrite
    for i in range(len(toHtml)):
        for j in range(len(toHtml[i])):
            if j <= len(toHtml[i])-1 and i <= len(toHtml)-2:
                prevEnd = getMaxEnd(toHtml[i])
                curStart = getMinStart(toHtml[i+1])
                if int(prevEnd[0]) > int(curStart[0]):

                    prevEndTime = prevEnd[0]
                    nextStartTime = curStart[0]
                    prevEndIdx = prevEnd[1]
                    nextStartIdx = curStart[1]

                    prevStart = toHtml[i][prevEndIdx][0].split()[9]
                    prevEnd = toHtml[i][prevEndIdx][0].split()[11]
                    nextStart = toHtml[i+1][nextStartIdx][0].split()[9]
                    nextEnd = toHtml[i+1][nextStartIdx][0].split()[11]

                    newPrevLine = ''
                    newNextLine = ''

                    for sub in range(len(toHtml[i][prevEndIdx][0].split())):

                        if sub == 9:
                            newPrevLine +=  str(prevStart) + ' '
                            newNextLine += str(nextStart) + ' '
                        elif sub == 11:
                            newPrevLine +=  str(nextStart) + ' '
                            newNextLine += str(prevEnd) + ' '
                        else:
                            newPrevLine += str(toHtml[i][prevEndIdx][0].split()[sub]) + ' '
                            newNextLine += str(toHtml[i][prevEndIdx][0].split()[sub]) + ' '

                    splitColor = toHtml[i][prevEndIdx][3]
                    toHtml[i][prevEndIdx] = [newPrevLine, toHtml[i][prevEndIdx][1], prevEndIdx, splitColor, toHtml[i][prevEndIdx][4]]
                    toHtml[i+1].append([newNextLine, toHtml[i][prevEndIdx][1], len(toHtml[i+1]), splitColor, toHtml[i+1][nextStartIdx][4]])

    for i in range(len(toHtml)):
        pageName = HTML_FILE_NAME + str(i+1) + ".html"
        outFile = open(pageName, 'a')
        numSys = getNumSysEDTs(dataCopy[i])
        numUsr = getNumUsrEDTs(dataCopy[i])

        if flag == SINGLE_PD or flag == DIST_FIRST_PD:
            if flag == SINGLE_PD:
                preFP = (open('htmlTemplates/preHtml.html', 'r'))
                pre = preFP.readlines()
                appendPreHTML(outFile, pre)
                preFP.close()
            elif flag == DIST_FIRST_PD:
                preFP = (open('htmlTemplates/preDistHtml.html', 'r'))
                pre = preFP.readlines()
                appendPreDistHTML(outFile, pre, pdNum, toHtml[i], numSys)
                preFP.close()

            appendWorkerIdHTML(outFile, uniqWorkers)

        else:
            outFile.write('<hr>')
            outFile.write('<p><b>Policy Domain ID: '+str(pdNum-1)+'</b></p>\n')
            outFile.write("<p>Num user EDTs executed: "+str(numUsr)+"</p>\n")
            outFile.write("<p>Num distributed system generated EDTs executed: "+str(numSys)+"</p>\n")
            outFile.write('<hr>')
            outFile.write('<p><b>&nbsp;&nbsp;Workers</b></p>\n')
            outFile.write('<div id=\"visualization'+str(pdNum)+'\"></div>\n')
            outFile.write('<script>\n')
            outFile.write('\tvar groups = new vis.DataSet([\n')
            appendWorkerIdHTML(outFile, uniqWorkers)

        uniqIds = []
        for j in range(len(toHtml[i])):
            lastLineFlag = False
            if j == len(toHtml[i]) - 1:
                lastLineFlag = True
            if toHtml[i][j][2] not in uniqIds:
                uniqIds.append(toHtml[i][j][2])
                postProcessData(toHtml[i][j][0], outFile, timeOffset, lastLineFlag, toHtml[i][j][2], toHtml[i][j][3])
            else:
                postProcessData(toHtml[i][j][0], outFile, timeOffset, lastLineFlag, (toHtml[i][j][2]+max_edts_per_page+len(uniqIds)), toHtml[i][j][3])

        startWindow = getMinStart(toHtml[i])
        endWindow = getMaxEnd(toHtml[i])
        if flag == SINGLE_PD:
            appendPostHTML(outFile, toHtml[i][j][4], i+1, startWindow[0], endWindow[0], timeOffset, numThreads)
        else:
            appendIntermediateHTML(outFile, toHtml[i][j][4], i+1, startWindow[0], endWindow[0], timeOffset, numThreads, flag, pdNum)

        outFile.close()

#======== Produce timeline in the case of distributed run ==========
def createDistributedTimeline(nodes, dbgLog, exeTime):
    global max_edts_per_page
    numNodes = len(nodes)
    pdCount = 0
    mappedColors = []
    for curPd in nodes:
        pdCount += 1
        with open(str(curPd), 'r') as inFile:
            lines = inFile.readlines()
            if(len(lines) == 0):
                print "Error: No EDT Names found in debug log.  Be sure proper flags are set and OCR has been rebuilt"
                cleanup(1)
                sys.exit(0)

            sortedLines = sortAscendingStartTime(lines)
            uniqWorkers = getUniqueWorkers(sortedLines)
            uniqNames = getUniqueFctNames(sortedLines)
            if len(mappedColors) == 0:
                mappedColors = assignColors(uniqNames)
            numThreads = len(uniqWorkers)
            sortedWrkrs = sortByWorker(sortedLines, uniqWorkers)

            numEDTs = len(sortedLines)

            numPages = numEDTs/max_edts_per_page
            if(numPages == 0):
                numPages = 1

            firstLine = sortedLines[0].split()
            timeOffset = int(firstLine[START_TIME_INDEX])
            lastLine = sortedWrkrs[-1]
            totalTime = int(sortedLines[-1].split()[END_TIME_INDEX]) - timeOffset

            toHtml = []
            for i in xrange(1, numPages+1):
                if numPages == 1:
                    flag = "single"
                elif i == 1:
                    flag = "first"
                elif i == numPages:
                    flag = "last"
                else:
                    flag = "mid"

                curPage = getSub(sortedLines, flag, i)
                localUniqWrkrs = getUniqueWorkers(curPage)
                localUniqNames = getUniqueFctNames(curPage)
                localSortedWrkrs = sortByWorker(curPage, localUniqWrkrs)
                localTimeSorted = sortAscendingStartTime(curPage)
                localTotalTime = int(localTimeSorted[-1].split()[END_TIME_INDEX]) - timeOffset
                localLastLine = localSortedWrkrs[-1]

                counter = 0
                localDataSet = []
                for line in localSortedWrkrs:
                    lastLineFlag = False
                    curFctName = line.split()[FCT_NAME_INDEX]
                    curColor = getColor(curFctName, mappedColors)
                    if line is localLastLine:
                        lastLineFlag = True
                    localDataSet.append([line, lastLineFlag, counter, curColor, flag])
                    counter += 1
                toHtml.append(localDataSet)

        if(pdCount == 1):
            preCheckEdtLengthAndWrite(toHtml, exeTime, numThreads, numEDTs, totalTime, uniqWorkers, timeOffset, DIST_FIRST_PD, pdCount)
        elif(pdCount > 1 and pdCount < numNodes):
            preCheckEdtLengthAndWrite(toHtml, exeTime, numThreads, numEDTs, totalTime, uniqWorkers, timeOffset, DIST_MID_PD, pdCount)
        else:
            preCheckEdtLengthAndWrite(toHtml, exeTime, numThreads, numEDTs, totalTime, uniqWorkers, timeOffset, DIST_LAST_PD, pdCount)

        #Print to inform User of progress
        print 'HTML data for PD:' + str(curPd) + ' Finished... ' + str((len(nodes) - pdCount)) + ' remaining'

#======== Produce timeline in the case of single node run ==========
def createSingleNodeTimeline(dbgLog):
    with open (STRIPPED_RECORDS, 'r') as inFile:

        lines = inFile.readlines()
        if len(lines) == 0:
            print "Error: No EDT Names found in debug log.  Be sure proper flags are set and OCR has been rebuilt"
            sys.exit(0)

        sortedLines = sortAscendingStartTime(lines)

        #Grab various statistcs/Arrange Log for postprocess
        exeTime = getExecutionTime(sortedLines)
        uniqWorkers = getUniqueWorkers(sortedLines)
        uniqNames = getUniqueFctNames(sortedLines)
        mappedColors = assignColors(uniqNames)
        numThreads = len(uniqWorkers)
        sortedWrkrs = sortByWorker(sortedLines, uniqWorkers)
        numEDTs = len(sortedLines)

        numPages = numEDTs/max_edts_per_page
        if numPages == 0:
            numPages = 1

        firstLine = sortedLines[0].split()
        timeOffset = int(firstLine[START_TIME_INDEX])
        lastLine = sortedWrkrs[-1]
        totalTime = int(sortedLines[-1].split()[END_TIME_INDEX]) - timeOffset

        toHtml = []
        #Subdivide EDT records into multiple pages if need be
        for i in xrange(1, numPages+1):
            if numPages == 1:
                flag = "single"
            elif i == 1:
                flag = "first"
            elif i == numPages:
                flag = "last"
            else:
                flag = "mid"

            curPage = getSub(sortedLines, flag, i)
            localUniqWrkrs = getUniqueWorkers(curPage)
            localUniqNames = getUniqueFctNames(curPage)
            localSortedWrkrs = sortByWorker(curPage, localUniqWrkrs)
            localTimeSorted = sortAscendingStartTime(curPage)
            localTotalTime = int(localTimeSorted[-1].split()[END_TIME_INDEX]) - timeOffset
            localLastLine = localSortedWrkrs[-1]

            counter = 0

            localDataSet = []
            for line in localSortedWrkrs:
                lastLineFlag = False
                curFctName = line.split()[FCT_NAME_INDEX]
                curColor = getColor(curFctName, mappedColors)
                if line is localLastLine:
                    lastLineFlag = True
                localDataSet.append([line, lastLineFlag, counter, curColor, flag])
                counter += 1

            toHtml.append(localDataSet)

        #Check for overlap, write to file
        preCheckEdtLengthAndWrite(toHtml, exeTime, numThreads, numEDTs, totalTime, uniqWorkers, timeOffset, 0, None)

    cleanup(0)
    inFile.close()
    sys.exit(0)

#======== cleanup temporary files =========
def cleanup(flag):
    os.remove(STRIPPED_RECORDS)
    if flag == 1:
        for f in os.listdir(os.getcwd()):
            if(re.search("0x", f)):
                os.remove(os.path.join(os.getcwd(), f))

#========USAGE========
def usage():
    print '\nERROR:  Unexpected argument(s)\n'
    print 'Usage:  python timeline.py <log_filename> <flags>\n'
    print 'flag options:'
    print '\t -s  :  Display distributed system generated EDTs (off by default)'
    print '\t -c  :  Combine subsequent EDTs of equal type (off by default)'
    print '\t -f  :  Force All EDTs to display on a single page (off by default)\n\n'
    sys.exit(0)

#=========MAIN==========
def main():

    global user_flag_sys
    global user_flag_combine
    global max_edts_per_page
    global user_flag_force_single

    if len(sys.argv) < 2 or len(sys.argv) > 5: usage()

    dbgLog = sys.argv[1]
    argList = []

    if len(sys.argv) > 2:
        for i in xrange(2, len(sys.argv)):
            a = sys.argv[i]
            if(a != '-s' and a != '-c' and a != '-f'): usage()
        if '-s' in sys.argv: user_flag_sys = True
        if '-c' in sys.argv: user_flag_combine = True
        if '-f' in sys.argv: user_flag_force_single = True

    #Get rid of events we are not interested in seeing
    if user_flag_force_single == True:
        max_edts_per_page = HUGE_CONSTANT

    if user_flag_sys == True:
        runShellStripDist(dbgLog)
    else:
        runShellStrip(dbgLog)
    #Get Number of Policy Domains present in logfile
    nodes = getNodes(STRIPPED_RECORDS)

    if(len(nodes) > 1):
        if user_flag_force_single == False:
            max_edts_per_page = (max_edts_per_page/(len(nodes)))
        nodeList = list(nodes)
        list.sort(nodeList)

        allFP = open(STRIPPED_RECORDS, 'r')
        lines = allFP.readlines()
        if len(lines) == 0:
            print "Error: No EDT Names found in debug log.  Be sure proper flags are set and OCR has been rebuilt"
            cleanup(0)
            sys.exit(0)
        sortedLines = sortAscendingStartTime(lines)
        if user_flag_force_single == True:
            print '\nWarning: Forcing single page may cause timeline to load slowly in browser\n'
        if len(sortedLines) > 10000:
            print 'Post-processing ' + str(len(sortedLines)) + ' EDTs... Large logs may take several minutes'
        exeTime = getExecutionTime(sortedLines)
        createFilePerPD(nodes, STRIPPED_RECORDS)
        createDistributedTimeline(nodeList, dbgLog, exeTime)
        print '\nDone!'
        cleanup(1)
        sys.exit(0)

    else:
        createSingleNodeTimeline(dbgLog)
        cleanup(0);
        sys.exit(0)

main();


