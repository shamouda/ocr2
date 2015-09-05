# Following script parses all stat files ( 1 per build )
# present in archive dir and generates an input file
# that contains trend line statistics . This is fed
# to plotgraph.py which generates trendline plots
#
# Generated stat file has following format:
# testname1 buildNo:time1,buildNo:time2,buildNo:time3
# testname2 buildNo:time1,buildNo:time2
# .
# .
# .

import sys
import os
import datetime
import csv

# Import perftest_baseval dict which contains values
# against which execution times will be normalized
# Each entry in perfTestBaseval.py looks like :
# {'testname':value_used_for_normalization,}
import perfTestBaseval

def main():
    if len(sys.argv) !=3 :
        print("ERROR ! Correct Usage :python statFileFolderParser archiveDir outStatFile\n")
        sys.exit(1)

    archiveDir = sys.argv[1]
    oFile = sys.argv[2]
    print("IN PYTHON archiveDir %s\n" % archiveDir);
    statFileList = os.listdir(archiveDir)
    statFileList.sort()
    testdict = dict()
    print("IN PYTHON 33 %s\n" % oFile);
    print("statFileList %s \n" % statFileList);
    for filename in statFileList:
        fd = open(os.path.join(archiveDir,filename), 'r')
        reader = csv.reader(fd)
        print("process filename %s\n" % os.path.join(archiveDir,filename));
        # x axis labels. Jenkins plot will reflect build number with date , manual won't.
        if (os.getenv("WORKSPACE")):
            # Jenkins running infra
            modifiedFileName = os.path.splitext(filename)[0]
        else:
            modifiedFileName = os.path.splitext(filename)[0]
            #TODO: commented this out because it doesn't do shit
            # modifiedFileName = "b#" + os.path.splitext(filename)[0] + "(" + datetime.datetime.now().strftime("%b%d") + ")"
        print("modifiedFileName %s\n" % modifiedFileName);
        # Extract test data from the stat file ( one per build ) in Archive dir
        for row in reader:
            print("row %s\n" % row);
            print("row[0] %s\n" % row[0]);
            print("row[1] %s\n" % row[1]);
            print ("perfTestBaseval %s\n" % perfTestBaseval)
            print ("perfTestBaseval.perftest_baseval %s\n" % perfTestBaseval.perftest_baseval)
            key                    = row[0]                      # Test name - Key
            filenameExecTimeTuple  = (modifiedFileName , (float(row[1])/float(perfTestBaseval.perftest_baseval[key]))) # Value of key , Normalized runtimes
            print("perfTestBaseval.perftest_baseval[key] %s\n" % perfTestBaseval.perftest_baseval[key]);
            print("(float(row[1])/float(perfTestBaseval.perftest_baseval[key])) %s\n" % (float(row[1])/float(perfTestBaseval.perftest_baseval[key])));
            print("key: %s\n", key);
            print("time: %s\n", filenameExecTimeTuple);
            if key not in testdict:
               testdict.setdefault(key,[filenameExecTimeTuple])
               print("key not in\n");
            else:
               testdict[key].append(filenameExecTimeTuple)
               print("key in\n");
        fd.close()
        print("done with  %s\n" % modifiedFileName);

    # Purge old output file , if exist
    if os.path.isfile(oFile):
        os.remove(oFile)

    print("open oFile %s\n" % oFile);
    fd = open(oFile, 'w')
    for key in testdict:
        buildtimeTupleList = []
        buildList = []
        execTimeList = []
        for ele in testdict[key]:
            buildList.append(ele[0])
            execTimeList.append(str(ele[1]))
        fd.write(key + " " + ",".join(buildList) + " " + ",".join(execTimeList)+'\n')
    fd.close()
    print("close oFile %s\n" % oFile);

if __name__ == '__main__':
    main()
