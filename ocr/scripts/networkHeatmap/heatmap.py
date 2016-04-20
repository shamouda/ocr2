import sys
import os
import math
import numpy
import argparse

import Tkinter as tk
from operator import itemgetter

TTYPE_INDEX = 14
SRC_INDEX = 20
DST_INDEX = 23
SIZE_INDEX = 26
MAR_TIME_INDEX = 29
SEND_TIME_INDEX = 32
RCV_TIME_INDEX = 35
UMAR_TIME_INDEX = 38
PD_INDEX = 5
MSG_TYPE_INDEX = 41

TK_WIN_WIDTH  = 1200
TK_WIN_HEIGHT = 900

FIRST_PASS = 0
RESET = 1
UPDATE = 2

verbose = 0

#define globals for simplicity
#All OCR message types. WARNING: Keep in sync with ocr-policy-domain.h
#Commented masks
ocrMessageTypes = [
# ["PD_MSG_INVAL",                        "0"],
# ["PD_MSG_DB_OP",                        "0x1"],
["PD_MSG_DB_CREATE",                    "0x51001"],
["PD_MSG_DB_DESTROY",                   "0x82001"],
["PD_MSG_DB_ACQUIRE",                   "0x23001"],
["PD_MSG_DB_RELEASE",                   "0x54001"],
["PD_MSG_DB_FREE",                      "0x85001"],
# ["PD_MSG_MEM_OP",                       "0x2"],
["PD_MSG_MEM_ALLOC",                    "0x401002"],
["PD_MSG_MEM_UNALLOC",                  "0x82002"],
# ["PD_MSG_WORK_OP",                      "0x4"],
["PD_MSG_WORK_CREATE",                  "0xe1004"],
["PD_MSG_WORK_EXECUTE",                 "0x42004"],
["PD_MSG_WORK_DESTROY",                 "0x83004"],
# ["PD_MSG_EDTTEMP_OP",                   "0x8"],
["PD_MSG_EDTTEMP_CREATE",               "0x51008"],
["PD_MSG_EDTTEMP_DESTROY",              "0x82008"],
# ["PD_MSG_EVT_OP",                       "0x10"],
["PD_MSG_EVT_CREATE",                   "0x51010"],
["PD_MSG_EVT_DESTROY",                  "0x82010"],
["PD_MSG_EVT_GET",                      "0x243010"],
# ["PD_MSG_GUID_OP",                      "0x20"],
["PD_MSG_GUID_CREATE",                  "0x11020"],
["PD_MSG_GUID_INFO",                    "0x12020"],
["PD_MSG_GUID_METADATA_CLONE",          "0x13020"],
["PD_MSG_GUID_RESERVE",                 "0x4020"],
["PD_MSG_GUID_UNRESERVE",               "0x5020"],
["PD_MSG_GUID_DESTROY",                 "0x46020"],
# ["PD_MSG_SCHED_OP",                     "0x40"],
["PD_MSG_SCHED_GET_WORK",               "0x1040"],
["PD_MSG_SCHED_NOTIFY",                 "0x2040"],
["PD_MSG_SCHED_TRANSACT",               "0x3040"],
["PD_MSG_SCHED_ANALYZE",                "0x4040"],
["PD_MSG_SCHED_UPDATE",                 "0x5040"],
["PD_MSG_COMM_TAKE",                    "0x6040"],
["PD_MSG_COMM_GIVE",                    "0x7040"],
# ["PD_MSG_DEP_OP",                       "0x80"],
["PD_MSG_DEP_ADD",                      "0xc1080"],
["PD_MSG_DEP_REGSIGNALER",              "0x82080"],
["PD_MSG_DEP_REGWAITER",                "0x83080"],
["PD_MSG_DEP_SATISFY",                  "0x104080"],
["PD_MSG_DEP_UNREGSIGNALER",            "0x85080"],
["PD_MSG_DEP_UNREGWAITER",              "0x86080"],
["PD_MSG_DEP_DYNADD",                   "0x87080"],
["PD_MSG_DEP_DYNREMOVE",                "0x88080"],
# ["PD_MSG_SAL_OP",                       "0x100"],
["PD_MSG_SAL_PRINT",                    "0x1100"],
["PD_MSG_SAL_READ",                     "0x2100"],
["PD_MSG_SAL_WRITE",                    "0x3100"],
["PD_MSG_SAL_TERMINATE",                "0x4100"],
# ["PD_MSG_MGT_OP",                       "0x200"],
["PD_MSG_MGT_REGISTER",                 "0x1200"],
["PD_MSG_MGT_UNREGISTER",               "0x2200"],
["PD_MSG_MGT_MONITOR_PROGRESS",         "0x3200"],
["PD_MSG_MGT_RL_NOTIFY",                "0x4200"],
# ["PD_MSG_HINT_OP",                      "0x400"],
["PD_MSG_HINT_SET",                     "0x41400"],
["PD_MSG_HINT_GET",                     "0x42400"],
# ["PD_MSG_TYPE_ONLY",                    "0xFFFFFFUL"],
# ["PD_MSG_META_ONLY",                    "0xFF000000UL"],
# ["PD_MSG_FG_IO_COUNT_ONLY",             "0x30000UL"],
# ["PD_MSG_FG_I_COUNT_ONLY",              "0x1C0000UL"],
# ["PD_MSG_FG_O_COUNT_ONLY",              "0xE00000UL"],
# ["PD_MSG_REQUEST",                      "0x1000000"],
# ["PD_MSG_RESPONSE",                     "0x2000000"],
["PD_MSG_REQ_RESPONSE",                 "0x4000000"],
["PD_MSG_RESPONSE_OVERRIDE",            "0x8000000"],
# ["PD_MSG_IGNORE_PRE_PROCESS_SCHEDULER", "0x10000000"],
# ["PD_MSG_REQ_POST_PROCESS_SCHEDULER",   "0x20000000"],
# ["PD_MSG_LOCAL_PROCESS",                 "0x4000000"]
]

ocrMessageTypesRev = {}

#Array to store src/dest pair message counts
networkMsgs = [[]]
#Array to store uniq nodes (PDs)
networkNodes = []
#Array to store message log records
filtered = []
#Array to store set of selected message type subset.
userDefMsgTypes = []
#Message type dictionary for faster lookup on update
msgTypeDict = [[]]
#Message counts to be displayed in matrix
msgCounts = [[]]

def calculateHeatColor(minV, maxV, val):
    ratio = 2*(val-(float(minV))) / (float(maxV) - float(minV))
    r = int(max(0, 255*(1-ratio)))
    b = int(max(0, 255*(ratio-1)))
    g = 255-b-r
    return r, g, b

class memMap(object):

    def redraw(self, onIndices):
        maxCount = 0
        minCount = sys.maxint

        self.canvas.update_idletasks()
        self.canvas.pack_forget()
        self.canvas.destroy()

    def __init__(self, master, **kwargs):
        global networkNodes
        global networkMsgs
        global ocrMessageTypes
        frame = tk.Frame(master)

        #Initialize Canvas
        self.canvas = tk.Canvas(master, width=TK_WIN_WIDTH, height=TK_WIN_HEIGHT, borderwidth=0, highlightthickness=0)
        self.canvas.pack(side="top", fill="both", expand="true")
        self.rows = len(networkNodes)
        self.columns = len(networkNodes)
        self.cellwidth = (TK_WIN_WIDTH/float(len(networkNodes)+1))
        self.cellheight = (TK_WIN_HEIGHT/float(len(networkNodes)+1))
        self.checked = []
        self.rect = {}
        self.text = {}

        master2 = tk.Tk()
        frame2 = tk.Frame(master2)

        #Setup dimensions for checkbox frame
        xDim = math.ceil(math.sqrt(float(len(ocrMessageTypes))))
        yDim = int(xDim) - 2
        xDim = int(yDim) + 3

        #Setup Tkinter IntVar()s for each message type checkbox
        self.intVars = []
        for i in range(len(ocrMessageTypes)):
            self.intVars.append(tk.IntVar());

        count = 0
        #Intitialize all checkboxes and apply them to a GUI fram
        for i in range(int(xDim)):
            for j in range(int(yDim)):
                if count >= len(ocrMessageTypes):
                    break

                curStr = ocrMessageTypes[count][0]
                chkBox = tk.Checkbutton(frame2, text=curStr, onvalue=1, offvalue=0, variable=self.intVars[count], command=self.populateChecked(count))
                chkBox.grid(row=i, column=j, sticky=tk.W)
                count += 1

        #Setup update button
        button = tk.Button(text="Update", command=self.updateHeatmap)
        frame2.pack()
        button.pack()

        #Display heatmap
        self.drawHeatmap(networkNodes, networkMsgs)

    def getCount(self, src, dst):
        global msgCounts
        global msgTypeDict
        global networkNodes
        global ocrMessageTypes
        global verbose

        if (verbose > 0):
            # iterate over this
            text=""
            for i in msgTypeDict[src][dst]:
                val = msgTypeDict[src][dst][i]
                count = len(val)
                if count != 0:
                    text+=ocrMessageTypesRev[i]
                    text+=": "
                    text+=str(len(val))
                    text+="\n"
            text+="TOTAL="
            text+=str(msgCounts[src][dst])
            return text
        else:
            return str(msgCounts[src][dst])

    def drawHeatmap(self, networkNodes, networkMsgs):
        global msgCounts
        maxCount = 0
        minCount = sys.maxint
        #Find MIN and MAX for heat color reference
        for i in xrange(len(networkNodes)):
            for j in xrange(len(networkNodes)):
                if(i == j): continue
                if(msgCounts[i][j] > maxCount):
                    maxCount = msgCounts[i][j]
                if(msgCounts[i][j] < minCount):
                    minCount = msgCounts[i][j]

        #Draw each grid square, apply number value, and calculate heat color
        for column in range(len(networkNodes)+1):
            for row in range(len(networkNodes)+1):
                x1 = column*self.cellwidth
                y1 = row * self.cellheight
                x2 = x1 + self.cellwidth
                y2 = y1 + self.cellheight
                if(row == 0 or column == 0):
                    self.rect[row,column] = self.canvas.create_rectangle(x1,y1,x2,y2, fill="gray", tags="rect")
                    if(row == 0 and column == 0):
                        self.text[row,column] = self.canvas.create_text((x2+x1)/2, (y2+y1)/2, text="", tags = "text")
                    elif(row == 0 and column != 0):
                        self.text[row,column] = self.canvas.create_text((x2+x1)/2, (y2+y1)/2, text="PD:0x"+str(column-1), tags = "text")
                    else:
                        self.text[row,column] = self.canvas.create_text((x2+x1)/2, (y2+y1)/2, text="PD:0x"+str(row-1), tags = "text")
                else:
                    if(row == column):
                        self.rect[row,column] = self.canvas.create_rectangle(x1,y1,x2,y2, fill="grey", tags="rectMax")
                        self.text[row,column] = self.canvas.create_text((x2+x1)/2, (y2+y1)/2, text=self.getCount(row-1,column-1), tags = "text")
                    else:
                        if(msgCounts[row-1][column-1] == 0):
                            heatColor = 'grey'
                        else:
                            heatColor = '#%02x%02x%02x' % (calculateHeatColor(maxCount, minCount, msgCounts[row-1][column-1]))
                        if(msgCounts[row-1][column-1] == maxCount):
                            self.rect[row,column] = self.canvas.create_rectangle(x1,y1,x2,y2, fill=heatColor, tags="rectMax")
                        elif(msgCounts[row-1][column-1] == minCount):
                            self.rect[row,column] = self.canvas.create_rectangle(x1,y1,x2,y2, fill=heatColor, tags="rectMin")
                        else:
                            self.rect[row,column] = self.canvas.create_rectangle(x1,y1,x2,y2, fill=heatColor, tags="rect2")
                        self.text[row,column] = self.canvas.create_text((x2+x1)/2, (y2+y1)/2, text=self.getCount(row-1, column-1), tags = "text")

    def populateChecked(self, idx):
        #Populate list checkboxed message types.
        def _populateChecked():
            if idx in self.checked:
                self.checked.remove(idx)
            else:
                self.checked.append(idx)
        return _populateChecked


    def updateHeatmap(self):

        global userDefMsgTypes
        global ocrMessageTypes

        #Redraw the heatmap with new grid values based on user selected message types.
        if len(self.checked) > 0:

            del userDefMsgTypes[:]
            for i in self.checked:
                userDefMsgTypes.append(ocrMessageTypes[i][1])

            messageNetwork(filtered, networkNodes, UPDATE)
            self.drawHeatmap(networkNodes, networkMsgs)

        else:
            del userDefMsgTypes[:]
            messageNetwork(filtered, networkNodes, RESET)
            self.drawHeatmap(networkNodes, networkMsgs)

            return

def initializeDictionaries():
    global msgTypeDict
    global networkNodes

    msgTypeDict = [[{} for node in networkNodes] for node in networkNodes]

    for i in range(len(networkNodes)):
        for j in range(len(networkNodes)):
            for k in ocrMessageTypes:
                msgType = k[1]
                msgTypeDict[i][j][msgType] = []


def getUniqPolicies(log):
    global networkNodes

    allPds = []
    for line in log:
        allPds.append(int(line.split()[SRC_INDEX], 0))
        allPds.append(int(line.split()[DST_INDEX], 0))

    networkNodes = sorted(set(allPds))

    retArr = []
    for i in networkNodes:
        retArr.append("0x"+str(i))

    return set(retArr)


def strip(dbgLog):
    filtered = []

    for i in dbgLog:
        if len(i.split()) == 42 and i.split()[TTYPE_INDEX] == "MESSAGE":
            filtered.append(i)

    return filtered


def netTime(msgs):
    excelData = open('timeData.csv', 'w')
    averages = open('averageTimes.txt', 'w')

    #Arrange file to properly open in excel and add heading
    excelData.write('sep=,\n')
    excelData.write(' ,PreNetwork,overNetwork,postNetwork\n')

    preNetTotal = []
    overNetTotal = []
    postNetTotal = []
    endToEndTotal = []

    for i in xrange(len(msgs)):

        if len(msgs[i].split()) != 42:
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

        excelData.write(csvString + ',' + msgs[i].split()[MSG_TYPE_INDEX] + '\n')

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


def messageNetwork(log, nodes, flag):

    global networkMsgs
    global userDefMsgTypes
    global ocrMessageTypes
    global ocrMessageTypesRev
    global msgTypeDict
    global msgCounts

    msgHexVals = []

    if flag == FIRST_PASS or flag == RESET:
        #First pass, setup display for all messages.
        if flag == FIRST_PASS:
            msgCounts = [[0 for node in networkNodes] for node in networkNodes]
            for line in log:
                src = int(line.split()[SRC_INDEX][2:])
                dst = int(line.split()[DST_INDEX][2:])
                msgType = line.split()[MSG_TYPE_INDEX]
                msgTypeDict[src][dst][msgType].append(line)
                msgCounts[src][dst] += 1
            for entry in ocrMessageTypes:
                ocrMessageTypesRev[entry[1]] = entry[0]
            return

        #Reset, occurs if some checkboxes were flipped on and subsequently all flipped off.
        zeroed = [[0 for node in networkNodes] for node in networkNodes]
        msgCounts = zeroed
        for msg in ocrMessageTypes:
            curType = msg[1]
            for i in range(len(networkNodes)):
                for j in range(len(networkNodes)):
                    msgCounts[i][j] += len(msgTypeDict[i][j][curType])

    #Update, occur if user flipped checkboxes for specific message types.  Display only those.
    else:
        msgCounts = [[0 for node in networkNodes] for node in networkNodes]
        for msg in userDefMsgTypes:
            curType = msg
            for i in range(len(networkNodes)):
                for j in range(len(networkNodes)):
                    msgCounts[i][j] += len(msgTypeDict[i][j][curType])

    return

#==================== MAIN =========================


def main():
    global userDefMsgTypes
    global TK_WIN_WIDTH
    global TK_WIN_HEIGHT
    global filtered
    global networkNodes
    global verbose

    parser = argparse.ArgumentParser(description='Network HeatMap')
    parser.add_argument('-x', '--x', dest='width', default='1920', help='width')
    parser.add_argument('-y', '--y', dest='height', default='720', help='height')
    parser.add_argument('-i', '--i', dest='fileName', default='', help='trace to process')
    parser.add_argument('--verbose', '-v', dest='verbose', action='count',
                   help='verbose mode')

    args = parser.parse_args()
    width = args.width
    height = args.height
    fileName = args.fileName
    verbose = args.verbose
    if (verbose is None):
        verbose = 0
    if fileName == '':
        fileName = raw_input("Enter filename: ")

    if width != "":
        if int(width) > 600:
            TK_WIN_WIDTH  = int(width)
        if int(height) > 600:
            TK_WIN_HEIGHT = int(height)

    fp = open(fileName, 'r')
    allMsgs = fp.readlines()

    #Filter Messages in the log
    filtered = strip(allMsgs)

    pds = getUniqPolicies(filtered)

    initializeDictionaries()
    # Below function is currently not very useful. Could be amped up in the future.
    #netTime(filtered)
    #print "Average time synopsis/CSV file written..."

    #Arrange sender/reciever counters among Policy domains.
    messageNetwork(filtered, list(pds), FIRST_PASS)
    print "Constructing message network"

    #Setup Tkinter and enter main loop for GUI
    root = tk.Tk()
    mmap = memMap(root)
    root.mainloop()

    fp.close()

main()

