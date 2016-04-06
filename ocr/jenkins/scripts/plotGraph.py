# Plots trendlines for user supplied statistics
# Can be used to generate trend line for any data.
# given input file is laid out in  following format -
#
# testname1 buildNo:time1,buildNo:time2,buildNo:time3
# testname2 buildNo:time1,buildNo:time2
# .
# .
#
# build no correspond to xasis labels and time will be
# y axis labels.

import numpy as np
import matplotlib as mpl
mpl.use('Agg')
import matplotlib.pyplot as plt
import pylab
import subprocess
import sys
import os
import random

#
# This file is duplicated across apps/apps/jenkins/scripts and
#                                ocr/ocr/jenkins/scripts
# Keep the two in sync manually
#

#-------------------------------------------------------------------------------
# Plots curve
#-------------------------------------------------------------------------------
def plotCurve(testname,stat,xrange, yMin, yMax, baseColor):
    plt.plot(xrange , stat,color = baseColor ,label=testname)
    plt.axhline(y=1,linestyle=':')
    plt.axvline(x=0)
    x1,x2,y1,y2 = plt.axis()
    plt.axis((x1,x2,float(yMin),float(yMax)))

#-------------------------------------------------------------------------------
# Plots markers
#-------------------------------------------------------------------------------
def plotAvgMarkers(stat,xrange, baseColor):
    plt.plot(xrange ,stat, marker='x', linestyle='None', color = baseColor)


#-------------------------------------------------------------------------------
# Plots trendline
#-------------------------------------------------------------------------------
def plotTrendLines(stat, xrange , baseColor):
    if len(xrange) != 1 :
        mBase,bBase = np.polyfit(xrange,stat,1)
        plt.plot(xrange, np.array(xrange) * mBase +bBase, color =baseColor)

#-------------------------------------------------------------------------------
# Inserts labels
#-------------------------------------------------------------------------------
def plotLabels(xLabel , yLabel , title ):
        plt.xlabel(xLabel)
        plt.ylabel(yLabel)
        plt.title(title)

#-------------------------------------------------------------------------------
# Inserts lagends
#-------------------------------------------------------------------------------
def plotLegends():
        plt.legend(prop={'size':6},loc='upper left')

#-------------------------------------------------------------------------------
# Generates an image file containing plots of the data from stat file
#-------------------------------------------------------------------------------
def plotgraph(testname,buildList , exectimeList, yMin, yMax, baseColor = 'b'):

    stat          = exectimeList
    buildNo       = buildList
    xaxisCount    = range(len(buildList))[0:len(buildList)]
    baseColor     = (random.random(),random.random(),random.random())
    stat = map(float, stat)

    xlabel = plt.gca()
    xlabel.set_xticks(xaxisCount)
    xlabel.set_xticklabels(buildNo)
    xlabel.xaxis.grid(True)
    plt.setp(xlabel.get_xticklabels(), rotation='vertical', fontsize=8)
    # Plot Curve
    plotCurve(testname,stat,xaxisCount,yMin,yMax,baseColor)

def main():
    if len(sys.argv) !=8 :
        print("ERROR ! Correct Usage :python plotGraph.py inputStatfile plotName xlabel ylabel outputFileName\n")
        sys.exit(1)

    iFilePath    = sys.argv[1]
    plotname     = sys.argv[2]
    xlabel       = sys.argv[3]
    ylabel       = sys.argv[4]
    yMin         = sys.argv[5]
    yMax         = sys.argv[6]
    oFilePath    = sys.argv[7]

    fd = open(iFilePath, 'r')
    plotFileList = []
    testNameList = []
    # Idea is to find the test having maximum no. of builds as it will
    # determine range of x axis in the plot
    testName_buildListSize_tup = (0,'',[])
    # Generate a dict(testStat) where test names are keys and value is
    # dictionary of stats for each test(buildStat). Dictionary of stats
    # - build no's are keys and exec. time is value.
    testStat = dict()
    for line in fd:
        # Every line contains stats for one test
        # Test_name comma_separated_builds comma_separated_stats
        # cholesky build#1,build#2,build#3 1.7,1.66,1.69
        buildList    = []
        exectimeList = []
        testName     = line.split()[0]
        buildList    = line.split()[1].split(',')
        exectimeList = line.split()[2].split(',')
        buildStat = dict()
        for index,build in enumerate(buildList):
           buildStat[build] =   exectimeList[index]
        testStat[testName] = buildStat

        if len(buildList) > testName_buildListSize_tup[0]:
            testName_buildListSize_tup = (len(buildList),testName,buildList)

    fd.close()
    buildList = testName_buildListSize_tup[2]

    # ---- Plotting Curve ----

    # Plotting labels
    plotLabels( xlabel , ylabel , plotname )
    for test in testStat :
        modExectimeList = []
        # Get build stat dictionary
        buildStat = testStat[test]
        for build in buildList:
            modExectimeList.append(float(buildStat[build]) if (build in buildStat) else 0.0)
        plotgraph(test,buildList,modExectimeList,yMin,yMax)

    # Plot legend
    plotLegends()

    # Purge old plot file , if exist
    if os.path.isfile(oFilePath):
        os.remove(oFilePath)

    plt.savefig(oFilePath,dpi=200,figsize=(7,3))
    plt.clf()

if __name__ == '__main__':
    main()
