#!/usr/bin/python
#
# Instruction/energy stat collector
# written by Wooil Kim
# kim844@illinois.edu
# last updated on Feb 9, 2015
#
# [CURRENT]
# Timing info is printed at block level using CE stat.
# Instruction info is printed at block level.
# Energy info is printed at block level, but energy numbers are all 0 for now.
#
# [TODO]
# Energy numbers need to be confirmed.
# print info for XE level may need to be implemented if required (such as load balance stats)
#

import sys
import re


# global settings
# dirname, blk, xe can be updated with command line arguments.
dirname = "logs_m500t100"
filename_prefix = "ocr-cholesky.log.brd00.chp00.unt00"
unit = 1
blk = 4
xe = 8


# global variables
ceTimingList = []

xeInstr = []
xeLocalRead = []
xeRemoteRead = []
xeLocalWrite = []
xeRemoteWrite = []

blkInstr = []
blkLocalRead = []
blkRemoteRead = []
blkLocalWrite = []
blkRemoteWrite = []

unitInstr = []
unitLocalRead = []
unitRemoteRead = []
unitLocalWrite = []
unitRemoteWrite = []

agentTotalEnergy = []
agentStaticEnergy = []
agentDynamicEnergy = []
agentMemoryEnergy = []
agentNetworkEnergy = []

blkTotalEnergy = []
blkStaticEnergy = []
blkDynamicEnergy = []
blkMemoryEnergy = []
blkNetworkEnergy = []

unitTotalEnergy = []
unitStaticEnergy = []
unitDynamicEnergy = []
unitMemoryEnergy = []
unitNetworkEnergy = []



# CE file is used for timing.
def ceProcessing(filename):
    file = open(dirname + "/" + filename, "r")
    for line in file:
        if (line.find("Timing Information") >= 0):
            #print line
            searchObj = re.search( r'logical clock: (.*)', line, re.M )
            #print searchObj.group(1)
    if searchObj:
        ceTimingList.append( int(searchObj.group(1)) )
    #print searchObj.group(1)
    file.close()



# XE file is used for instruction-related stats.
def xeProcessing(filename):
    file = open(dirname + "/" + filename, "r")
    for line in file:
        if (line.find("LOCAL_READ_COUNT") >= 0):
            searchObj = re.search( r'LOCAL_READ_COUNT : (.*)', line, re.M )
            if searchObj:
                xeLocalRead.append( int(searchObj.group(1)) )
        if (line.find("REMOTE_READ_COUNT") >= 0):
            searchObj = re.search( r'REMOTE_READ_COUNT : (.*)', line, re.M )
            if searchObj:
                xeRemoteRead.append( int(searchObj.group(1)) )
        if (line.find("LOCAL_WRITE_COUNT") >= 0):
            searchObj = re.search( r'LOCAL_WRITE_COUNT : (.*)', line, re.M )
            if searchObj:
                xeLocalWrite.append( int(searchObj.group(1)) )
        if (line.find("REMOTE_WRITE_COUNT") >= 0):
            searchObj = re.search( r'REMOTE_WRITE_COUNT : (.*)', line, re.M )
            if searchObj:
                xeRemoteWrite.append( int(searchObj.group(1)) )
        if (line.find("INSTRUCTIONS_EXECUTED") >= 0):
            searchObj = re.search( r'INSTRUCTIONS_EXECUTED : (.*)', line, re.M)
            if searchObj:
                xeInstr.append( int(searchObj.group(1)) )
    file.close()
    #print "end of xeProcessing"



# stdout file is used for energy-related stats.
def stdoutProcessing(filename):
    file = open(dirname + "/" + filename, "r")
    for line in file:
        if (line.find("Agent #") >= 0):
            #searchObj = re.search( r'Agent #(.*) \((.*):(.*):(.*)\):', line, re.M )
            #print line
            searchObj = re.search( r'Agent #(.*) \((.*):(.*):(.*)\):', line, re.M )
            '''
            if searchObj:
                # [TODO] need to handle this information
                print searchObj
                print searchObj.group(1)
                print searchObj.group(2)
                print searchObj.group(3)
                print searchObj.group(4)
            '''
        elif (line.find("Approximate energy usage") >= 0):
            #Approximate energy usage: 0.000000 (static: 0.000000, dynamic: 0.000000, network: 0.000000)
            #print line
            searchObj = re.search( r'Approximate energy usage: (.*) uJ \(static: (.*) uJ, dynamic: (.*) uJ, memory: (.*) uJ, network: (.*) uJ\)', line, re.M )
            if searchObj:
                #print searchObj
                #print searchObj.group(1)
                #print searchObj.group(2)
                #print searchObj.group(3)
                #print searchObj.group(4)
                agentTotalEnergy.append( float(searchObj.group(1)) )
                agentStaticEnergy.append( float(searchObj.group(2)) )
                agentDynamicEnergy.append( float(searchObj.group(3)) )
                agentMemoryEnergy.append( float(searchObj.group(4)) )
                agentNetworkEnergy.append( float(searchObj.group(5)) )

    blkTotalEnergy.append(sum(agentTotalEnergy));
    blkStaticEnergy.append(sum(agentStaticEnergy));
    blkDynamicEnergy.append(sum(agentDynamicEnergy));
    blkMemoryEnergy.append(sum(agentMemoryEnergy));
    blkNetworkEnergy.append(sum(agentNetworkEnergy));

    del agentTotalEnergy[:]
    del agentStaticEnergy[:]
    del agentDynamicEnergy[:]
    del agentMemoryEnergy[:]
    del agentNetworkEnergy[:]

    file.close()



def fileProcessing():
    #for u in range (0, unit):
        for b in range (0, blk):
            #filename = filename_prefix + ".unt%02d" % u + ".blk%02d" % b + ".CE.%02d" % 0
            filename = filename_prefix + ".blk%02d" % b + ".CE.%02d" % 0
            print "processing " + filename
            ceProcessing(filename)

    #for u in range (0, unit):
        for b in range (0, blk):
            for x in range (0, xe):
                #filename = filename_prefix + ".unt%02d" % u + ".blk%02d" % b + ".XE.%02d" % x
                filename = filename_prefix + ".blk%02d" % b + ".XE.%02d" % x
                print "processing " + filename
                xeProcessing(filename)
                '''
                # print for XE-level stat is blocked now.
                print xeInstr
                print xeLocalRead
                print xeRemoteRead
                print xeLocalWrite
                print xeRemoteWrite
                '''

            blkInstr.append(sum(xeInstr))
            blkLocalRead.append(sum(xeLocalRead))
            blkRemoteRead.append(sum(xeRemoteRead))
            blkLocalWrite.append(sum(xeLocalWrite))
            blkRemoteWrite.append(sum(xeRemoteWrite))

            del xeInstr[:]
            del xeLocalRead[:]
            del xeRemoteRead[:]
            del xeLocalWrite[:]
            del xeRemoteWrite[:]

    #for u in range (0, unit):
        for b in range (0, blk):
            #filename = filename_prefix + ".unt%02d" % u + ".blk%02d" % b + ".stdout"
            filename = filename_prefix + ".blk%02d" % b + ".stdout"
            print "processing " + filename
            stdoutProcessing(filename)



def argumentProcessing():
    global dirname
    global filename_prefix
    global unit
    global blk
    global xe

    if (len(sys.argv) == 6):
        dirname = sys.argv[1]
        filename_prefix = sys.argv[2]
        unit = int(sys.argv[3])
        blk = int(sys.argv[4])
        xe = int(sys.argv[5])

        print "Arguments are given as follows:"
        print "dirname: ", dirname
        print "filename prefix: ", filename_prefix
        print "unit: ", unit
        print "blk: ", blk
        print "xe: ", xe
        print
    elif (len(sys.argv) == 1):
        print "No argument is given. Use default parameters:"
        print "dirname: ", dirname
        print "filename prefix: ", filename_prefix
        print "unit: ", unit
        print "blk: ", blk
        print "xe: ", xe
        print
    else:
        print "Error in arguments."
        print "You gave %d arguments, but 6 arguments including collect.py are required." % len(sys.argv)
        print "python collect.py [dirname filename_prefix number_of_units number_of_blocks number_of_XEs]"
        print "example 1) python collect.py"
        print "example 2) python collect.py logs_m500t100 ocr-cholesky 1 4 8"
        print "=== collect.py ends ==="
        print
        sys.exit(-1)




####################################
# implicit main function start
####################################
print "=== collect.py starts ==="
argumentProcessing()

# CE, XE, stdout file processing
fileProcessing()

# final results
print
print "timestamp per block: ", ceTimingList
print
print "instruction per block: ", blkInstr
print "local read per block: ", blkLocalRead
print "remote read per block: ", blkRemoteRead
print "local write per block: ", blkLocalWrite
print "remote write per block: ", blkRemoteWrite
print
print "total energy per block: ", blkTotalEnergy
print "static energy per block: ", blkStaticEnergy
print "dynamic energy per block: ", blkDynamicEnergy
print "memory energy per block: ", blkMemoryEnergy
print "network energy per block: ", blkNetworkEnergy
print
print "=== collect.py ends ==="
print

