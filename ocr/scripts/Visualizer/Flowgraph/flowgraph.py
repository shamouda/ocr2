#
# This file is subject to the license agreement located in the file
# LICENSE and cannot be distributed without it.  This notice
# cannot be removed or modified
#


import sys

try:
    sys.path.append('/opt/pygraph-1.8.2/python-graph-core-1.8.2')
except:
    e = sys.exc_info()[0]
    print("pygraph package not found in python path.  Check /opt and make sure python-graph-core is there.\n\n"

)
import pygraph
from pygraph.classes.digraph import digraph
from pygraph.algorithms.critical import transitive_edges, critical_path, find_cycle
import os
import subprocess
import collections
import math
from operator import itemgetter
HTML_FILE_NAME = 'flowgraph.html'

#================ Fill EDT node html dataSet ===================
def writeEdtCreations(edts, outFile, nameMap, levels):
    lastLine = edts[-1]
    outFile.write('\tedtNodeData = [\n')

    for line in edts:
        edtGuid = line
        edtName = edtGuidToName(edtGuid, nameMap)
        curLevel = getCurrentLevelHelper(edtGuid, levels)
        if curLevel == None:
            curLevel = 0
        if line is lastLine:
            outFile.write('\t\t{id: ' + '\'' + str(edtGuid) + '\'' + ', label: \'' + str(edtName) + '\', title: \'EDT Name: ' + str(edtName) + '\' + \'<br>\' + \'GUID: ' + str(edtGuid) + '\', value: 20, color: \'LightSkyBlue\', level: ' + str(curLevel) + ', allowedToMoveX: true, allowedToMoveY: true}\n')
            outFile.write('\t];\n\n')
        elif edtName == 'mainEdt':
            outFile.write('\t\t{id: ' + '\'' + str(edtGuid) + '\'' + ', label: \'' + str(edtName) + '\', title: \'EDT Name: ' + str(edtName) + '\' + \'<br>\' + \'GUID: ' + str(edtGuid) + '\', value: 20, color: \'LightSkyBlue\', level: ' + str(curLevel) + ',  allowedToMoveX: true, allowedToMoveY: true},\n')
        else:
            outFile.write('\t\t{id: ' + '\'' + str(edtGuid) + '\'' + ', label: \'' + str(edtName) + '\', title: \'EDT Name: ' + str(edtName) + '\' + \'<br>\' + \'GUID: ' + str(edtGuid) + '\', value: 20, color: \'LightSkyBlue\', level: ' + str(curLevel) + ', allowedToMoveX: true, allowedToMoveY: true},\n')

#================ Fill event node html dataset ==================
def writeEventCreations(events, outFile, eventMap, levels):
    lastLine = events[-1]
    outFile.write('\teventNodeData = [\n')
    for line in events:
        evtGuid = line
        evtType = evtGuidToType(evtGuid, eventMap)
        curLevel = getCurrentLevelHelper(evtGuid, levels)
        if curLevel == None:
            curLevel = 0
        if line is lastLine:
            outFile.write('\t\t{id:' + '\'' + str(evtGuid) + '\'' + ', label: \'Event\', title: \'Event type: \' + \'' + str(evtType) + '\' + \'<br>\' + \'GUID: \' + \'' + str(evtGuid) + '\', value: 20, color: \'Tomato\', level: ' + str(curLevel) + ', allowedToMoveX: true, allowedToMoveY: true}\n')
            outFile.write('\t];\n\n')
        else:
            outFile.write('\t\t{id:' + '\'' + str(evtGuid) + '\'' + ', label: \'Event\', title: \'Event type: \' + \'' + str(evtType) + '\' + \'<br>\' + \'GUID: \' + \'' + str(evtGuid) + '\', value: 20, color: \'Tomato\', level: ' + str(curLevel) + ', allowedToMoveX: true, allowedToMoveY: true},\n')

#============= Fill Datablack node html dataset ================
def writeDbCreations(allDbs, outFile, dbLvls):
    outFile.write('\tdatablockNodeData = [\n')
    prevObserved = []

    for i in range(len(allDbs)):
        curDb = allDbs[i]
        curLvl = getCurrentLevelHelper(curDb, dbLvls)
        if curLvl == None:
            print "DB not being assigned a level.  Debug this."
            curLvl = 0
        if i == len(allDbs) - 1:
            outFile.write('\t\t{id:' + '\'' + str(curDb) + '\'' + ', label: \'DataBlock\', title: \'GUID: \' + \'' + str(curDb) + '\', value: 20, shape: \'square\', color: \'Green\', level: ' +str(curLvl) + ', allowedToMoveX: true, allowedToMoveY: true}\n')
            outFile.write('\t];\n\n')
        else:
            outFile.write('\t\t{id:' + '\'' + str(curDb) + '\'' + ', label: \'DataBlock\', title: \'GUID: \' + \'' + str(curDb) + '\', value: 20, shape: \'square\', color: \'Green\', level: ' +str(curLvl) + ', allowedToMoveX: true, allowedToMoveY: true},\n')

#================ Fill edge html dataset =======================
def writeSatEdges(sats, outFile, critPath):
    lastEdge = sats[-1]
    outFile.write('\tsatEdgeData = [\n')
    pairs = []
    pathDrawn = []
    for line in sats:
        src = line[0]
        dst = line[1]
        inCritPath = any([src, dst] == critPath[i:i+2] for i in xrange(len(critPath)-1))

        if inCritPath == True and line is lastEdge:
            outFile.write('\t\t{from: ' + '\'' + str(src) + '\'' + ', to: ' + '\'' + str(dst) + '\', value: 2, color: \'Lime\'' + '}\n')
            outFile.write('\t];\n\n')
            continue

        elif inCritPath == True and line is not lastEdge:
            outFile.write('\t\t{from: ' + '\'' + str(src) + '\'' + ', to: ' + '\'' + str(dst) + '\', value: 2, color: \'Lime\'' + '},\n')
            continue

        elif line is lastEdge:
            outFile.write('\t\t{from: ' + '\'' + str(src) + '\'' + ', to: ' + '\'' + str(dst) + '\', value: 1' + '}\n')
            outFile.write('\t];\n\n')
            continue

        else:
            outFile.write('\t\t{from: ' + '\'' + str(src) + '\'' + ', to: ' + '\'' + str(dst) + '\', value: 1' + '},\n')

#============ Write datablock satisfaction edges ==============
def writeDbSatEdges(dbSats, outFile):
    outFile.write('\tdbSatEdgeData = [\n')
    for i in range(len(dbSats)):
        src = dbSats[i][1]
        dst = dbSats[i][0]
        if i == len(dbSats)-1:
            outFile.write('\t\t{from: ' + '\'' + str(src) + '\'' + ', to: ' + '\'' + str(dst) + '\', value: 1, length: 1' + '}\n')
            outFile.write('\t];\n\n')
        else:
            outFile.write('\t\t{from: ' + '\'' + str(src) + '\'' + ', to: ' + '\'' + str(dst) + '\', value: 1, length: 1' + '},\n')


#========== Read in html templates and write to outputHtml ===========
def writePreHtml(outFile, pre):
    for line in pre:
        outFile.write(line)

def writePostHtml(outFile, post, exeTime, critTime, pathLen):
    for line in post:
        outFile.write(line)
    outFile.write("<p>Total execution time: " + str(exeTime) + "ns</p>\n")
    outFile.write("<p>Cummulative EDT execution time along critical path: " + str(critTime) + "ns</p>\n")
    outFile.write("<p>Critical path length: " + str(pathLen) + "</p>\n")
    outFile.write("</body>\n")
    outFile.write("</html>\n")

#========= Getters for log data to be postprocessed ===============
def getApiEdts(edts):
    edtGuids = []
    for line in edts:
        if line.split()[9][-1] == ';':
            edtGuids.append(line.split()[9][:-1])
        else:
            edtGuids.append(line.split()[9])
    return edtGuids

def getCommonEdts(edts):
    edtGuids = []
    for line in edts:
        edtGuids.append(line.split()[5])
    return edtGuids


def getApiDbGuids(Dbs):
    dbGuids = []
    for line in Dbs:
        dbGuids.append(line.split()[6][5:-1])
    return dbGuids

def getCommonDbGuids(Dbs):
    dbGuids = []
    for line in Dbs:
        dbGuids.append(line.split()[8])
    return dbGuids

def getApiDbPairs(Dbs):
    satPairs = []
    for line in Dbs:
        curLine = line.split()
        curObj = curLine[5][24:-1]
        curDb = curLine[6][5:-1]
        satPairs.append((curObj, curDb))
    return satPairs

def getCommonDbPairs(Dbs):
    satPairs = []
    for line in Dbs:
        curLine = line.split()
        curObj = curLine[6]
        curDb = curLine[8]
        satPairs.append((curObj, curDb))
    return satPairs

def getApiEvts(evts):
    evtGuids = []
    for line in evts:
        evtGuids.append(line.split()[9])
    return evtGuids

def getCommonEvts(evts):
    evtGuids = []
    for line in evts:
        evtGuids.append(line.split()[6])
    return evtGuids

def getApiSats(sats):
    satPairs = []
    for line in sats:
        curLine = line.split()
        satPairs.append((curLine[3][4:-1], curLine[5][24:-1]))
    return satPairs

def getCommonSats(sats):
    satPairs = []
    for line in sats:
        curLine = line.split()
        satPairs.append((curLine[3][4:-1], curLine[6]))
    return satPairs

def getFromSats(sats):
    satPairs = []
    for line in sats:
        curLine = line.split()
        satPairs.append((curLine[6], curLine[8]))
    return satPairs

#=============== Generate key-val data structure for EDT guids to name =========
def getNameMap(names):
    nameMap = []
    for line in names:
        curLine = line.split()
        name = curLine[7]
        edt = curLine[5]
        nameMap.append([edt, name])
    return nameMap

#============= Generatle key-val data structure for EDT guids to exe time ==========
def getTimeMap(names):
    timeMap = []
    for line in names:
        curLine = line.split()
        edt = curLine[5]
        startTime = float(curLine[9])
        endTime = float(curLine[11])
        time = endTime - startTime
        timeMap.append([edt, time])
    return timeMap

#============ More verbose version of getTimeMap w/ start and end times =============
def getRunTimes(names):
    runTimeMap = []
    sortedNames = sortAscendingStartTime(names)
    offSet = float(sortedNames[0].split()[9])
    for i in sortedNames:
        curLine = i.split()
        #Structure: each record has EDT name(0) guid(1) startTime(2) and endTime(3)
        curStart = (float(curLine[9]) - offSet)
        curEnd = (float(curLine[11])- offSet)
        runTimeMap.append([curLine[7], curLine[5], curStart, curEnd])
    return runTimeMap

#=========Account for (possible) out of order log writes by sorting=====
def sortAscendingStartTime(logFile):
    splitLog = []
    sortedList = []
    for line in logFile:
        splitLog.append(line.split())

    for element in splitLog:
        element[9] = int(element[9])

    tempSorted = sorted(splitLog, key=itemgetter(9))

    for element in tempSorted:
        element[9] = str(element[9])

    for element in tempSorted:
        sortedList.append((' '.join(element))+'\n')

    return sortedList

#======== Generate key-val data struture for guid to event type =========
def getEvtTypeMap(comEvts, apiEvts):

    evtTypeMap = []
    for line in comEvts:
        evtGuid = line.split()[6]
        if 'once' in line or 'Once' in line:
            evtTypeMap.append([evtGuid, "Once"])
            continue
        if 'latch' in line or 'Latch' in line:
            evtTypeMap.append([evtGuid, "Latch"])
            continue
        if 'sticky' in line or 'Sticky' in line:
            evtTypeMap.append([evtGuid, "Sticky"])
            continue
        if 'idem' in line or 'Idem' in line:
            evtTypeMap.append([evtGuid, "Idempotent"])
            continue

    for line in apiEvts:
        evtGuid = line.split()[9]
        if 'once' in line or 'Once' in line:
            evtTypeMap.append([evtGuid, "Once"])
            continue
        if 'latch' in line or 'Latch' in line:
            evtTypeMap.append([evtGuid, "Latch"])
            continue
        if 'sticky' in line or 'Sticky' in line:
            evtTypeMap.append([evtGuid, "Sticky"])
            continue
        if 'idem' in line or 'Idem' in line:
            evtTypeMap.append([evtGuid, "Idempotent"])
            continue

    return evtTypeMap

#=========== Get Event type from guid =============
def evtGuidToType(guid, evtMap):
    for i in evtMap:
        if i[0] == guid:
            return i[1]

    return 'Runtime Event'

#============== Get name from EDT guid ==============
def edtGuidToName(guid, nameMap):
    for i in nameMap:
        if i[0] == guid:
            return i[1]

    return 'Runtime EDT'

#========= Returns time EDT guid took to execute =======
def edtGuidToTime(guid, timeMap):
    for i in timeMap:
        if i[0] == guid:
            return i[1]

    return 1

#============== Get mainEDT's guid =================
def getMainEdtGuid(nameMap):
    for i in nameMap:
        if i[1] == 'mainEdt':
            return i[0]

    return "Error, no Main EDT"

#=========== Check if guid has been assigned a hierarchy level =============
def isLevelDefinedHelper(curNode, levels):
    if any(curNode in x for x in levels) == True:
        return True
    else:
        return False

#=========== Getter for current Guid's Level ==================
def getCurrentLevelHelper(curNode, levels):
    retLevel = None
    for i in range(len(levels)):
        if levels[i][0] == curNode:
            retLevel = levels[i][1]
            break

    return retLevel

#========== Function to put main at first index of dataset =========
def moveEdtFront(guid, pairs):
    for i in range(len(pairs)):
        if pairs[i][0] == guid:
            pairs.insert(0, pairs.pop(i))
            return 1

    return 0

#========= Define hierarchy levels for DBs ============
def defDbLevels(dbPairs, objLvls):
    dbLvls = []
    for i in range(len(dbPairs)):
        curObjLvl = getCurrentLevelHelper(dbPairs[i][0], objLvls)
        curDb = dbPairs[i][1]
        dbLvls.append([curDb, curObjLvl+1])
    return dbLvls


#========= Define node leveling heirarchy for visualization =========
def defineHierarchy(edgePairs, runtimeMap, allEdts, allEvts, critPath):
    srcs = []
    dsts = []
    levels = []

    for i in range(len(edgePairs)):
        srcs.append(edgePairs[i][0])
        dsts.append(edgePairs[i][1])

    allNodes = srcs+dsts
    for i in allNodes:
        if i not in srcs and i not in dsts:
            allNodes.pop(allNodes.index(i))

    numNodes = len(set(allNodes))

    #set main to lvl 0
    for i in range(len(runtimeMap)):
        if runtimeMap[i][0] == 'mainEdt':
            levels.append([runtimeMap[i][1], 0])
            moveEdtFront(runtimeMap[i][1], edgePairs)
            break

    #Set levels for each component in critical path
    for i in range(len(critPath)):
        levels.append([critPath[i], (i+1)])
    flag = False

    while(len(levels) < numNodes):
        prevLen = len(levels)
        for i in range(len(edgePairs)):
            curSrc = edgePairs[i][0]
            curDst = edgePairs[i][1]

            #Define initial levels with simple hueristic
            if isLevelDefinedHelper(curSrc, levels) == True and isLevelDefinedHelper(curDst, levels) == True:
                continue
            elif isLevelDefinedHelper(curSrc, levels) == True and isLevelDefinedHelper(curDst, levels) == False:
                srcLvl = getCurrentLevelHelper(curSrc, levels)
                levels.append([curDst, srcLvl+1])
            elif isLevelDefinedHelper(curSrc, levels) == False and isLevelDefinedHelper(curDst, levels) == True:
                dstLvl = getCurrentLevelHelper(curDst, levels)
                levels.append([curSrc, dstLvl-1])
            else:

                #Should not get hit in a proper OCR app. This accounts for sitatutions where a graph has multiple connected components.
                if flag == True:
                    levels.append([curSrc, 0])
                    levels.append([curDst, 1])
                    flag == False
                    break

        curLen = len(levels)
        if curLen == prevLen:
            flag = True

    return levels

#========= Checks, and modifies levels if src->dst arrow direction is not downward ========
def cleanupArrows(edgePairs, levels, allEdts, allEvts, mainGuid):
    srcs = []
    dsts = []
    lvlGuids = []
    lvlVals = []

    for i in range(len(edgePairs)):
        srcs.append(edgePairs[i][0])
        dsts.append(edgePairs[i][1])

    offset = 0
    for i in range(len(levels)):
        lvlGuids.append(levels[i][0])
        lvlVals.append(levels[i][1])
    minVal = min(lvlVals)

    if minVal < 0:
        offset = abs(minVal)+1

    counter = 0
    #Iterate until no incorrect arrow direction
    while(counter!= len(srcs)):
        incFlag = False
        for i in range(len(srcs)):
            counter += 1
            curSrc = srcs[i]
            curSrcLvl = getCurrentLevelHelper(curSrc, levels)
            adjDsts = []

            #Find all destination nodes from current source
            for j in range(len(srcs)):
                if srcs[j] == curSrc:
                    adjDsts.append(dsts[j])
            for j in range(len(adjDsts)):
                curDst = adjDsts[j]
                curDstLvl = getCurrentLevelHelper(curDst, levels)

                #Arrow incorrect, adjust destination levels
                if curDstLvl < curSrcLvl:
                    incFlag = True
                    curSrcLvl = curDstLvl
                    idx = lvlGuids.index(curSrc)
                    levels.pop(idx)
                    levels.insert(idx, [curSrc, curSrcLvl])

                    nextAdjDsts = []

                    #Adjust levels of next incident edges
                    for m in range(len(srcs)):
                        if srcs[m] == curDst:
                            nextAdjDsts.append(dsts[m])
                    for m in range(len(nextAdjDsts)):
                        nextDst = nextAdjDsts[m]
                        nextDstLvl = getCurrentLevelHelper(nextDst, levels)
                        if nextDstLvl < curDstLvl:
                            nextDstLvl = curDstLvl

                            idx2 = lvlGuids.index(nextDst)
                            levels.pop(idx2)
                            levels.insert(idx2, [nextDst, nextDstLvl])

        #Reset to zero, incorrect direction found
        if incFlag == True:
            counter = 0

    #If any level values went negative, increment all levels by offset
    if offset > 0:
        for i in range(len(levels)):
            levels[i][1] += offset

    #Ensure Main is at top of tree
    for i in range(len(levels)):
        if levels[i][0] == mainGuid:
            levels[i][1] = 0
            break

    return levels

#========== Assign all disconnected nodes value of 0 (top of graph) ============
def zeroDisconnectedNodes(levels):
    for i in range(len(levels)):
        if levels[i][1] == None:
            levels[i][1] = 0
    return levels

#======== Offset all defined levels to include datablocks between levels ==========
def offsetLevels(levels):
    for i in range(len(levels)):
        dblLvl = (int(levels[i][1]))*2
        levels[i][1] = dblLvl

    return levels

#========== Get's level of first connected node in execution flow ===========
def getFirstConnectedNode(allSats, levels):
    allLevels = []
    srcs = []
    for i in range(len(allSats)):
        srcs.append(allSats[i][0])
    for i in range(len(levels)):
        allLevels.append(levels[i][1])
    totalLvls = max(allLevels)
    for i in range(totalLvls):
        for j in range(len(levels)):
            if levels[j][1] == i and levels[j][0] in srcs:
                return i

    return 0

#========= Returns node type of given Guid ========
def getNodeType(guid, edts, evts):
    for i in evts:
        if i == guid:
            return 'Event'
    for i in edts:
        if i == guid:
            return 'Edt'

#========= Compute execution time =============
def getTotalExeTime(edts):
    startTime = float(edts[0].split()[9])
    endTime = float(edts[-1].split()[11])
    return endTime - startTime

#======= Compute critical path ==============
def pygraphCrit(edgePairs, nameMap, timeMap, allEdts, allEvts, mainGuid):
    G = digraph()
    srcs = []
    dsts = []
    for i in range(len(edgePairs)):
        srcs.append(edgePairs[i][0])
        dsts.append(edgePairs[i][1])

    allNodes = set(srcs+dsts)
    for i in allNodes:
        if i == mainGuid:
            continue
        else:
            G.add_node(str(i))
    for i in range(len(edgePairs)):
        curTime = 0
        curSrc = str(edgePairs[i][0])
        curDst = str(edgePairs[i][1])
        if curSrc == mainGuid or curDst == mainGuid:
            continue
        if getNodeType(curSrc, allEdts, allEvts) == 'Event':
            curTime = 1
        else:
            curTime = edtGuidToTime(curSrc, timeMap)
        G.add_edge((curSrc, curDst), curTime)

    if len(critical_path(G)) == 0:
        print 'Cycle Detected, exiting; Please check that the OCR conifiguration file uses GUID type of COUNTED_MAP, and the application has no cycles.'
        print 'Dumping cycle below.'
        print find_cycle(G)
        os.remove(HTML_FILE_NAME)
        sys.exit(0)

    return critical_path(G)

#======== Execution time of EDTs on critical path ========
def getCritPathExeTime(path, edtMap):
    edtGuids = []
    critTime = 0
    for i in range(len(edtMap)):
        edtGuids.append(edtMap[i][1])

    for i in range(len(path)):
        #curNode is Edt
        if path[i] in edtGuids:
            idx = edtGuids.index(path[i])
            curExe = edtMap[idx][3] - edtMap[idx][2]
        else:
            curExe = 0

        critTime += curExe

    return critTime

#========= Strip necessary records from debug logs with grep =========
def runShellStrip(dbgLog):
    os.system("egrep -w \'API\\(INFO\\)\' " + str(dbgLog) + " | grep \'EXIT ocrEventSatisfy\' > apiSats.txt")
    os.system("egrep -w \'EVT\\(INFO\\)\' " + str(dbgLog) + " | grep \'Satisfy \' > sats.txt")
    os.system("egrep -w \'API\\(INFO\\)\' " + str(dbgLog) + " | grep \'EXIT ocrEventCreate\' > apiEvts.txt")
    os.system("egrep -w \'EVT\\(INFO\\)\' " + str(dbgLog) + " | grep Create > evts.txt")
    os.system("egrep -w \'API\\(INFO\\)\' " + str(dbgLog) + " | grep \'EXIT ocrEdtCreate\' > apiEdts.txt")
    os.system("egrep -w \'API\\(INFO\\)\' " + str(dbgLog) + " | grep \'ocrEventSatisfySlot\' | grep ENTER > apiDbs.txt")
    os.system("egrep -w \'EVT\\(INFO\\)\' " + str(dbgLog) + " | grep \'Satisfy ' | grep with > Dbs.txt")
    os.system("egrep -w \'EDT\\(INFO\\)\' " + str(dbgLog) + " | grep Create > edts.txt")
    os.system("grep \'SatisfyFromEvent\' " + str(dbgLog) + " > fromSats.txt")
    os.system("grep FctName " + str(dbgLog) + " > names.txt")

#======== Remove temporarily created files =============
def cleanup():
    os.remove('apiSats.txt')
    os.remove('sats.txt')
    os.remove('apiEvts.txt')
    os.remove('evts.txt')
    os.remove('apiEdts.txt')
    os.remove('edts.txt')
    os.remove('names.txt')
    os.remove('fromSats.txt')
    os.remove('apiDbs.txt')
    os.remove('Dbs.txt')

#========= MAIN ==========
def main():

    #Get rid of events we are not interested in seeing
    if len(sys.argv) != 2:
        print "Error... Usage: python netformat.py <inputFile>"
        sys.exit(0)
    dbgLog = sys.argv[1]
    runShellStrip(dbgLog)

    #Open html templates, and create new html file for writing.
    outFile = open(HTML_FILE_NAME, 'w')
    preFP = open('htmlTemplates/preHtml.html', 'r')
    pre = preFP.readlines()
    postFP = open('htmlTemplates/postHtml.html', 'r')
    post = postFP.readlines()

    #Read in temp files produced by grep
    apiSatsFP = open('apiSats.txt', 'r')
    apiSats = apiSatsFP.readlines()
    satsFP = open('sats.txt', 'r')
    sats = satsFP.readlines()
    fromSatsFP = open('fromSats.txt', 'r')
    fromSats = fromSatsFP.readlines()
    apiEvtsFP = open('apiEvts.txt', 'r')
    apiEvts = apiEvtsFP.readlines()
    evtsFP = open('evts.txt', 'r')
    evts = evtsFP.readlines()
    apiEdtsFP = open('apiEdts.txt', 'r')
    apiEdts = apiEdtsFP.readlines()
    edtsFP = open('edts.txt', 'r')
    edts = edtsFP.readlines()
    namesFP = open('names.txt', 'r')
    names = namesFP.readlines()
    apiDbsFP = open('apiDbs.txt', 'r')
    apiDbs = apiDbsFP.readlines()
    DbsFP = open('Dbs.txt', 'r')
    Dbs = DbsFP.readlines()

    #Close file pointers and cleanup temp files
    apiSatsFP.close()
    satsFP.close()
    fromSatsFP.close()
    apiEvtsFP.close()
    evtsFP.close()
    apiEdtsFP.close()
    edtsFP.close()
    namesFP.close()
    apiDbsFP.close()
    DbsFP.close()
    cleanup()

    if len(names) == 0:
        print "Error: No EDT names found in debug log. Be sure proper flags are set and OCR has been rebuilt"
        os.remove(HTML_FILE_NAME)
        sys.exit(0)

    #Fetch GUIDs of EDTs and Events, and src->dst pairs from satisfaction log records
    apiSats2 = getApiSats(apiSats)
    sats2 = getCommonSats(sats)

    fromSats2 = getFromSats(fromSats)

    apiEvts2 = getApiEvts(apiEvts)
    evts2 = getCommonEvts(evts)

    apiEdts2 = getApiEdts(apiEdts)
    edts2 = getCommonEdts(edts)

    apiDbs2 = getApiDbGuids(apiDbs)
    dbs2 = getCommonDbGuids(Dbs)

    apiDbSats = getApiDbPairs(apiDbs)
    dbSats = getCommonDbPairs(Dbs)

    #Create utility data structures
    nameMap = getNameMap(names)
    evtTypeMap = getEvtTypeMap(evts, apiEvts)
    timeMap = getTimeMap(names)
    runTimesMap = getRunTimes(names)
    mainGuid = getMainEdtGuid(nameMap)
    totalExeTime = getTotalExeTime(names)

    #Remove duplicates
    allDbGuids = list(collections.OrderedDict.fromkeys(apiDbs2 + dbs2))
    allDbSats = list(collections.OrderedDict.fromkeys(apiDbSats+dbSats))
    allSats = list(collections.OrderedDict.fromkeys(apiSats2 + sats2 + fromSats2))
    allEdts = list(collections.OrderedDict.fromkeys(apiEdts2 + edts2))
    allEvts = list(collections.OrderedDict.fromkeys(apiEvts2 + evts2))

    #Compute critical path and level hierarchy for viz
    critPath = pygraphCrit(allSats, nameMap, timeMap, allEdts, allEvts, mainGuid)
    critPathLen = len(critPath)
    critTime =  getCritPathExeTime(critPath, runTimesMap)
    lvls = defineHierarchy(allSats, runTimesMap, allEdts, allEvts, critPath)
    modLvls = cleanupArrows(allSats, lvls, allEdts, allEvts, mainGuid)
    modLvls = zeroDisconnectedNodes(modLvls)
    modLvls = offsetLevels(modLvls)
    dbLvls  = defDbLevels(allDbSats, modLvls)
    firstPathNode = critPath[0]
    for i in range(len(modLvls)):
        if modLvls[i][0] == firstPathNode:
            modLvls[i][1] = getFirstConnectedNode(allSats, modLvls)
            break

    #Write postprocessed data to html file
    writePreHtml(outFile, pre)
    writeEdtCreations(allEdts, outFile, nameMap, modLvls)
    writeEventCreations(allEvts, outFile, evtTypeMap, modLvls)
    writeDbCreations(allDbGuids, outFile, dbLvls)
    writeDbSatEdges(allDbSats, outFile)
    writeSatEdges(allSats, outFile, critPath)
    writePostHtml(outFile, post, totalExeTime, critTime, critPathLen)

    outFile.close()
    preFP.close()
    postFP.close()

    sys.exit(0)


main();
