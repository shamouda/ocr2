#!/usr/bin/python

# Modified in 2014 by Romain Cledat (now at Intel). The original
# license (BSD) is below. This file is also subject to the license
# aggrement located in the OCR LICENSE file and cannot be distributed
# without it. This notice cannot be removed or modified

# Copyright (c) 2011, Romain Cledat
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of the Georgia Institute of Technology nor the
#       names of its contributors may be used to endorse or promote products
#       derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL ROMAIN CLEDAT BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import sys, getopt, re, math, copy, os, fnmatch
from operator import itemgetter
from cStringIO import StringIO

### Globals
funcNames = []
allEnsembles = dict() # key: Base Name for the ensemble "all" for the ensemble containing all threads;
                      # value: a EnsembleStatistics object
allThreads = dict() # key: Filename for the thread; value: a ThreadStatistics object

def flattenList(iterable):
    it = iter(iterable)
    for e in it:
        if isinstance(e, (list, tuple)):
            for f in flattenList(e):
                yield f
        else:
            yield e

class EnsembleStatistics(object):
    "Represents the statistics of several threads (either a node or all threads)"
    def __init__(self, *args):
        if len(args) == 1 and type(args[0]) == type(self):
            # "Copy" constructor. We do a deep copy
            # This is not very "python-y" but I was having issues with copies
            # of FuncEntry and the "container" link
            other = args[0]
            self.baseName = other.baseName
            self.nodeName = other.nodeName
            self.gatheringThread = ThreadStatistics(other.gatheringThread)
            for v in other.allThreads:
                self.allThreads.append(ThreadStatistics(v))
        elif len(args) == 2:
            self.baseName = args[0]
            self.nodeName = args[1]
            self.gatheringThread = None
            self.allThreads = []
        else:
            assert(0)

    def addThreadStatistics(self, thread):
        if self.gatheringThread is None:
            self.gatheringThread = ThreadStatistics(thread)
            self.gatheringThread.fileName = "_gatherthread_node_%s" % (self.nodeName)
            self.gatheringThread.threadName = "Node %s" % (self.nodeName)
        else:
            self.gatheringThread.addInstance(thread)
        self.allThreads.append(thread)

    def calculateStatistics(self):
        self.gatheringThread.calculateStatistics()

class ThreadStatistics(object):
    "Represents a thread of computation for which statistics are being gathered"
    def __init__(self, *args):
        self.funcEntries = dict()
        if len(args) == 1 and type(args[0]) == type(self):
            # "Copy" constructor
            other = args[0]
            self.fileName = other.fileName
            self.threadName = other.threadName
            for k, v in other.funcEntries.iteritems():
                self.funcEntries[k] = FuncEntry(v, self)
            self.instanceCount = other.instanceCount

        elif len(args) == 2:
            self.fileName = args[0]
            self.threadName = args[1]
            self.instanceCount = 1
        else:
            assert(0)

    def __getitem__(self, key):
        return getattr(self, key)

    def getFuncEntry(self, id, name = None):
        entry = self.funcEntries.get(id)
        if entry is None:
            assert(name is not None)
            entry = self.funcEntries[id] = FuncEntry(id, name, self)
            self.funcEntries[id] = entry
        return entry

    def calculateStatistics(self):
        for entry in self.funcEntries.values():
            entry.calculateStatistics()

    def addInstance(self, other):
        "Add an instance of ThreadStatistics to this one, merging their information"
        initialKeys = frozenset(self.funcEntries.keys())
        touchedKeys = set()
        for k, entry in other.funcEntries.iteritems():
            if k in self.funcEntries:
                self.funcEntries[k].addInstance(entry)
                touchedKeys.add(k)
            else:
                # We need to create the FuncEntry
                fEntry = self.getFuncEntry(k, entry.name)
                fEntry.set(0, 0, 0)
                # This says that for all threads we know about already
                # we didn't have this function so we count accordingly
                fEntry.addEmptyInstances(self.instanceCount - 1)
                fEntry.addInstance(entry)
        for k in initialKeys - touchedKeys:
            # These are function entries that do not exist
            # in the other thread
            self.funcEntries[k].addEmptyInstances(other.instanceCount)
        self.instanceCount += other.instanceCount

class EnsembleValue(object):
    "Represents a value that computes average and standard deviation on itself"
    def __init__(self, *args):
        if len(args) == 1:
            if type(args[0]) == type(self):
                # Copy
                other = args[0]
                self.__dict__.update(other.__dict__)
            else:
                value = args[0]
                self.total = value
                self.totalSq = value*value
                self.count = 1
                # Calculated values
                self.avg = 0.0
                self.stdDev = 0.0
                self.calculatedStatistics = False
        else:
            assert(0)

    def __add__(self, other):
        if type(other) == type(self):
            return self.total + other.total
        return self.total + total

    def __sub__(self, other):
        if type(other) == type(self):
            return self.total - other.total
        return self.total - other

    def __mul__(self, other):
        if type(other) == type(self):
            return self.total * other.total
        return self.total * other

    def __div__(self, other):
        if type(other) == type(self):
            return self.total / other.total
        return self.total / other

    def __truediv__(self, other):
        if type(other) == type(self):
            return self.total / other.total
        return self.total / other

    def __float__(self):
        return float(self.total)

    def __int__(self):
        return int(self.total)

    def __iadd__(self, other):
        self.addValue(other)
        return self

    def __lt__(self, other):
        if type(other) == type(self):
            return self.total < other.total
        return self.total < other

    def __le__(self, other):
        if type(other) == type(self):
            return self.total <= other.total
        return self.total <= other

    def __eq__(self, other):
        if type(other) == type(self):
            return self.total == other.total
        return self.total == other

    def __ne__(self, other):
        if type(other) == type(self):
            return self.total <> other.total
        return self.total <> other

    def __gt__(self, other):
        if type(other) == type(self):
            return self.total > other.total
        return self.total > other

    def __ge__(self, other):
        if type(other) == type(self):
            return self.total >= other.total
        return self.total >= other

    def getAvg(self):
        if not self.calculatedStatistics:
            self.calculateStatistics()
        return self.avg

    def getStdDev(self):
        if not self.calculatedStatistics:
            self.calculateStatistics()
        return self.stdDev

    def addValue(self, value, *other):
        multiplier = 1
        if len(other):
            multiplier = other[0]
        if type(self) == type(value):
            count = value.count
            value = value.total
        else:
            count = 1
        self.total += value * multiplier
        self.totalSq += value * value * multiplier
        self.count += count * multiplier
        self.calculatedStatistics = False

    def setValue(self, value):
        self.total = value
        self.totalSq = value * value
        self.count = 1
        self.calculatedStatistics = False

    def calculateStatistics(self):
        if self.calculatedStatistics:
            return
        self.avg = float(self.total)/self.count
        try:
            self.stdDev = math.sqrt(float(self.totalSq)/self.count - self.avg*self.avg)
        except ValueError:
            self.stdDev = 0.0 # Small value
        self.calculatedStatistics = True


class ChildEntry(object):
    "Represents a child of a top-level function being tracked by the profiler"
    def __init__(self, tlEntryId, tlEntryName, container, *args):
        self.parentId = None
        self.tlEntryId = tlEntryId
        self.tlEntryName = tlEntryName
        self.container = container
        self.entries = ('count', 'totalTime', 'totalTimeSq', 'totalTimeInChildren',
                        'totalTimeInChildrenSq', 'totalTimeRecurse', 'totalTimeRecurseSq')
        if len(args) == 1 and type(args[0]) == type(self):
            other = args[0]
            for k in self.entries:
                setattr(self, k, EnsembleValue(getattr(other, k)))
            # Calculated statistics
            self.totalTimeSelf = EnsembleValue(other.totalTimeSelf)
            self.totalTimeInChildrenNoRecurse = EnsembleValue(other.totalTimeInChildrenNoRecurse)
            self.avg = other.avg
            self.stdDev = other.stdDev
            self.calculatedStatistics = other.calculatedStatistics
        elif len(args) == 7:
            """You need to call addChild and addParent to link parent and child
                   - count: Number of times this child was called from the parent
                   - totalTime: Total amount of time spent in this child for this parent
                   - totalTimeSq: Square of totalTime
                   - totalTimeInChildren: Total amount of time spent in this child's children
                   - totalTimeInChildrenSq: Square of totalTimeInChildren
                   - totalTimeRecurse: Total time from this child that the parent function was called
                   - totalTimeRecurseSq: Square of totalTimeRecurse
            """
            # Statistics on this entry
            for k, i in zip(self.entries, range(0, 7)):
                setattr(self, k, EnsembleValue(args[i]))
            # Calculated statistics
            self.totalTimeSelf = EnsembleValue(args[1] - args[3])
            self.totalTimeInChildrenNoRecurse = EnsembleValue(args[3] - args[5])
            self.avg = 0.0
            self.stdDev = 0.0
            self.calculatedStatistics = False
        else:
            assert(0)

    def __getitem__(self, key):
        return getattr(self, key)

    def calculateStatistics(self):
        """Calculates useful statistics"""

        if self.calculatedStatistics:
            return
        self.avg = self.totalTime/self.count
        try:
            self.stdDev = math.sqrt(self.totalTimeSq/self.count - self.avg*self.avg)
        except ValueError:
            self.stdDev = 0.0 # Small value
        self.calculatedStatistics = True

    def addParent(self, parent):
        assert(self.parentId is None)
        self.parentId = parent.id
        self.container.getFuncEntry(self.tlEntryId, self.tlEntryName).addParent(parent)

    def addInstance(self, other):
        """Used to merge with another "same" entry (to group in an ensemble)
           This is used to build ensemble statistics"""
        assert(self.parentId == other.parentId and
               self.tlEntryId == other.tlEntryId)
        for k in self.entries:
            getattr(self, k).addValue(getattr(other, k))
        self.totalTimeSelf += other.totalTimeSelf
        self.totalTimeInChildrenNoRecurse += other.totalTimeInChildrenNoRecurse
        self.calculatedStatistics = False

    def addEmptyInstances(self, count):
        self.count.addValue(0, count)
        self.totalTime.addValue(0, count)
        self.totalTimeSq.addValue(0, count)
        self.totalTimeInChildren.addValue(0, count)
        self.totalTimeInChildrenSq.addValue(0, count)
        self.totalTimeSelf.addValue(0, count)
        self.totalTimeRecurse.addValue(0, count)
        self.totalTimeRecurseSq.addValue(0, count)
        self.calculatedStatistics = False

class FuncEntry(object):
    "Represents a top-level function that is being tracked by the profiler"

    def __init__(self, *args):
        self.entries = ('count', 'totalTime', 'totalTimeSq',
                        'totalTimeSelf', 'totalTimeInChildren',
                        'totalTimeRecurse', 'totalTimeNoRecurse',
                        'totalTimeInChildrenNoRecurse')

        if len(args) == 2 and type(args[0]) == type(self):
            # Copy constructor with new container
            other = args[0]
            self.container = args[1]
            self.copyFrom(other)
        elif len(args) == 3:
            self.id = args[0]
            self.name = args[1]
            self.container = args[2]
            self.isInit = False

            # Contains the parents (callers of this function)
            # This is a list of IDs for parents
            self.parentEntries = set()
            # Contains the children (functions called by this function)
            #  - Key: ID of the child (child.tlEntryId)
            #  - Value: ChildEntry of the child
            self.childrenEntries = { }

            # This is what needs to be filled in at some point
            self.count = None
            self.totalTime = None
            self.totalTimeSq = None

            # Calculated statistics
            self.totalTimeSelf = EnsembleValue(0.0)
            self.totalTimeInChildren = EnsembleValue(0.0)
            self.totalTimeRecurse = EnsembleValue(0.0)
            self.totalTimeNoRecurse = EnsembleValue(0.0)
            self.totalTimeInChildrenNoRecurse = EnsembleValue(0.0)

            self.avg = 0.0
            self.stdDev = 0.0
            self.calculatedStatistics = False
            self.multipleInstances = False

            # Other info
            self.rank = 0
        else:
            assert(False)

    def copyFrom(self, other):
        self.id = other.id
        self.name = other.name
        for k in self.entries:
            setattr(self, k, EnsembleValue(getattr(other, k)))
        # We can just copy these as they will be valid here too
        self.avg = other.avg
        self.stdDev = other.stdDev
        self.calculatedStatistics = other.calculatedStatistics
        self.multipleInstances = other.multipleInstances
        self.isInit = other.isInit
        self.rank = other.rank
        self.parentEntries = set()
        self.childrenEntries = {}
        self.parentEntries.update(other.parentEntries)
        for k, v in other.childrenEntries.iteritems():
            myChild = ChildEntry(k, v.tlEntryName, self.container, v)
            myChild.addParent(self)
            self.childrenEntries[k] = myChild

    def set(self, count, totalTime, totalTimeSq):
        assert(not self.isInit)
        self.count = EnsembleValue(count)
        self.totalTime = EnsembleValue(totalTime)
        self.totalTimeSq = EnsembleValue(totalTimeSq)

        self.totalTimeSelf.setValue(
            self.totalTime - self.totalTimeInChildren)
        self.totalTimeNoRecurse.setValue(self.totalTime.total)
        self.isInit = True

    def __getitem__(self, key):
        return getattr(self, key)

    def __repr__(self):
        return "%s [%d]" % (self.name, self.rank)

    def __cmp__(self, other):
        if self.id < other.id:
            return -1
        elif self.id == other.id:
            return 0
        return 1

    def addParent(self, tlParent):
        """Adds a parent."""
        self.parentEntries.add(tlParent.id)

    def addChild(self, childEntry):
        """Adds a child."""
        assert(not self.multipleInstances and \
               "Cannot add children after ensemble statistics have been added")
        t = self.childrenEntries.get(childEntry.tlEntryId)
        if t is not None:
            assert(t is childEntry)
        else:
            self.childrenEntries[childEntry.tlEntryId] = childEntry
            self.totalTimeInChildren.setValue(
                self.totalTimeInChildren + childEntry.totalTime)
            self.totalTimeRecurse.setValue(
                self.totalTimeRecurse + childEntry.totalTimeRecurse)
            self.totalTimeInChildrenNoRecurse.setValue(
                float(self.totalTimeInChildrenNoRecurse) +
                float(childEntry.totalTime) - float(childEntry.totalTimeRecurse))
            if self.isInit:
                self.totalTimeSelf.setValue(
                    self.totalTime - self.totalTimeInChildren)
                self.totalTimeNoRecurse.setValue(
                    self.totalTime - self.totalTimeRecurse)

    def calculateStatistics(self):
        """Calculates useful statistics (like time spent in children etc...)"""
        if self.calculatedStatistics:
            return
        assert(self.isInit)
        for child in self.childrenEntries.itervalues():
            child.calculateStatistics()

        self.avg = self.totalTime/self.count
        try:
            self.stdDev = math.sqrt(self.totalTimeSq/self.count - self.avg*self.avg)
        except ValueError:
            self.stdDev = 0.0
        self.calculatedStatistics = True

    def addInstance(self, other):
        """Used to merge with another "same" entry (to group in an ensemble)
        """
        assert(self.id == other.id)

        if not self.isInit:
            # This is basically the first init
            for k in self.entries:
                setattr(self, k, EnsembleValue(getattr(other, k)))
            self.isInit = True
        else:
            # Add another instance
            for k in self.entries:
                getattr(self, k).addValue(getattr(other, k))

        self.multipleInstances = True

        # This can be any count; we just need to know the number of
        # entries for the function. self.count.count == self.totalTime.count, etc.
        myInstancesCount = self.count.count
        otherInstancesCount = other.count.count

        # We need to make sure we update all children in some fashion
        initialKeys = frozenset(self.childrenEntries.keys())
        touchedKeys = set()
        for k, child in other.childrenEntries.iteritems():
            if k in self.childrenEntries:
                self.childrenEntries[k].addInstance(child)
                touchedKeys.add(k)
            else:
                # Create a childEntry
                myChild = ChildEntry(k, child.tlEntryName, self.container, 0, 0, 0, 0, 0, 0, 0)
                # Make sure the count is correct
                myChild.addEmptyInstances(myInstancesCount - 1) # We added one instance already
                myChild.addParent(self)
                myChild.addInstance(child) # Get all information from other child
                self.childrenEntries[k] = myChild
                # Recurse information was already updated
        for k in initialKeys - touchedKeys:
            # We did not update these children. It means they didn't exist in other
            # We update their count
            self.childrenEntries[k].addEmptyInstances(otherInstancesCount)

        self.calculatedStatistics = False

    def addEmptyInstances(self, count):
        self.multipleInstances = True

        for k in self.entries:
            getattr(self, k).addValue(0, count)
        for child in self.childrenEntries.itervalues():
            child.addEmptyInstances(count)


def printThreadInfo(headerLine, totalTime, flatSorted, callGraphSorted, fullStats=False, doRecurse=False):
    # NOTE: print "x", "y" will print "x y" so it inserts a space automatically!!!
    print headerLine
    print "\tTotal measured time: %f" % totalTime
    print "\t--- Flat profile ---"
    padding = ""
    padding1 = ""
    recurseEmptyField = ""
    widthOrigFloat = 16
    widthFloat = 16
    widthOrigInt = 12
    widthInt = 12
    if fullStats:
        print " ",
        widthFloat = widthFloat*2 + 4
        widthInt = widthFloat
        padding = " "*8
        padding1 = " "*12
    if doRecurse:
        recurseEmptyField = " "*(widthFloat + 2)
    if fullStats:
        print " ",
    print "%Time ", "", "Cum ms".center(widthFloat), "", recurseEmptyField, "", "Self ms".center(widthFloat), "", "Calls".center(widthInt), "", "Avg (Cum)".center(widthOrigFloat), "", "Std Dev (Cum)".center(widthOrigFloat)

    for e in flatSorted:
        if fullStats:
            print "/",
        print "{0:<5.2f} ".format(e.totalTimeSelf/totalTime*100), " {0:^{width}.6f} ".format(float(e.totalTimeNoRecurse), width=widthFloat),
        if doRecurse and e.totalTimeRecurse <> 0.0:
            print "[{0:.6f}]".format(float(e.totalTimeRecurse)).center(widthFloat+2),
        else:
            print recurseEmptyField,
        print " {0:^{width}.6f}".format(float(e.totalTimeSelf), width=widthFloat),
        print " {0:^{width}d}".format(int(e.count), width=widthInt),
        print " {0:^{width}.6f}  {1:^{width}.6f}  {2:s} {3:s} [{4:d}]".format(e.avg, e.stdDev, e.name, "R" if e.totalTimeRecurse <> 0 else " ", e.rank, width=widthOrigFloat)
        if fullStats:
            print "\\      ", "({0:.6f}, {1:.6f})".format(e.totalTimeNoRecurse.getAvg(), e.totalTimeNoRecurse.getStdDev()).center(widthFloat) + " ",
            if doRecurse and e.totalTimeRecurse <> 0.0:
                print "[({0:.6f}, {1:.6f})]".format(e.totalTimeRecurse.getAvg(), e.totalTimeRecurse.getStdDev()).center(widthFloat+2),
            else:
                print recurseEmptyField,
            print " " + "({0:.6f}, {1:.6f})".format(e.totalTimeSelf.getAvg(), e.totalTimeSelf.getStdDev()).center(widthFloat),
            print " " + "({0:.6f}, {1:.6f})".format(e.count.getAvg(), e.count.getStdDev()).center(widthInt)

    print "\n\n\t--- Call-graph profile ---"
    if fullStats:
        print " ",
    print "Index ", "%Time".center(6), "", "Self".center(widthFloat), "", "Descendant".center(widthFloat), "", recurseEmptyField, "", "Called".center(widthInt*2+1)
    for e in callGraphSorted:
        # First build a list of all the entries that refer to us in our parents
        myParentsChildrenEntry = []
        for pEntryId in e.parentEntries:
            pEntry = e.container.getFuncEntry(pEntryId)
            myParentsChildrenEntry.append((pEntry.childrenEntries[e.id], pEntry))

        # Print all parents
        for myChildEntry, myParent in iter(sorted(myParentsChildrenEntry, key=lambda i: i[0].totalTime, reverse=True)):
            if fullStats:
                print "/",
            print " "*14, "{0:^{width}.6f}".format(float(myChildEntry.totalTimeSelf), width=widthFloat),
            print " {0:^{width}.6f} ".format(float(myChildEntry.totalTimeInChildren), width=widthFloat),
            print recurseEmptyField,
            print " {0:>{width}d}".format(int(myChildEntry.count), width=widthOrigInt).center(widthInt) + \
                "/" + "{0:<{width}d}".format(int(e.count), width=widthOrigInt).center(widthInt),
            print " {0} [{1:d}]".format(myParent.name, myParent.rank)
            if fullStats:
                print "\\",
                print " "*14, "({0:.6f}, {1:.6f})".format(myChildEntry.totalTimeSelf.getAvg(), myChildEntry.totalTimeSelf.getStdDev()).center(widthFloat),
                print " " + "({0:.6f}, {1:.6f})".format(myChildEntry.totalTimeInChildren.getAvg(), myChildEntry.totalTimeInChildren.getStdDev()).center(widthFloat),
                print recurseEmptyField,
                print " " + "({0:.6f}, {1:.6f})".format(myChildEntry.count.getAvg(), myChildEntry.count.getStdDev()).center(widthInt) + \
                    "/" + "({0:.6f}, {1:.6f})".format(e.count.getAvg(), e.count.getStdDev()).center(widthInt)

        # Print our entry
        if fullStats:
            print "/",
        print "[{0:3d}]  {1:6.2f}  {2:^{width}.6f}  {3:^{width}.6f} ".format(
            e.rank, (e.totalTimeNoRecurse)/totalTime*100, float(e.totalTimeSelf),
            float(e.totalTimeInChildrenNoRecurse), width=widthFloat),
        if doRecurse and e.totalTimeRecurse <> 0:
            print "[{0:.6f}]".format(float(e.totalTimeRecurse)).center(widthFloat+2),
        else:
            print recurseEmptyField,

        print " " + "{0:^{width}d}".format(int(e.count), width=widthOrigInt).center(widthInt*2+1), " {0} [{1:d}]".format(e.name, e.rank)
        if fullStats:
            print "\\",
            print " "*14, "({0:.6f}, {1:.6f})".format(e.totalTimeSelf.getAvg(), e.totalTimeSelf.getStdDev()).center(widthFloat),
            print " " + "({0:.6f}, {1:.6f})".format(e.totalTimeInChildrenNoRecurse.getAvg(), e.totalTimeInChildrenNoRecurse.getStdDev()).center(widthFloat),
            if doRecurse and e.totalTimeRecurse <> 0:
                print "[({0:.6f}, {1:.6f})]".format(e.totalTimeRecurse.getAvg(), e.totalTimeRecurse.getStdDev()).center(widthFloat+2),
            else:
                print recurseEmptyField,
            print " " + "({0:.6f}, {1:.6f})".format(e.count.getAvg(), e.count.getStdDev()).center(widthInt*2+1)

        # Print children
        for cEntry in iter(sorted(e.childrenEntries.values(), key=lambda i: i.totalTimeSelf, reverse=True)):
            if fullStats:
                print "/",
            tlEntry = e.container.getFuncEntry(cEntry.tlEntryId)
            print " "*14, "{0:^{width}.6f}".format(float(cEntry.totalTimeSelf), width=widthFloat),
            print " {0:^{width}.6f} ".format(float(cEntry.totalTimeInChildrenNoRecurse), width=widthFloat),
            if doRecurse and cEntry.totalTimeRecurse <> 0:
                print "[{0:.6f}]".format(float(cEntry.totalTimeRecurse)).center(widthFloat+2),
            else:
                print recurseEmptyField,

            print " {0:>{width}d}".format(int(cEntry.count), width=widthOrigInt).center(widthInt) + \
                "/" + "{0:<{width}d}".format(int(tlEntry.count), width=widthOrigInt).center(widthInt), " {0} [{1:d}]".format(tlEntry.name, tlEntry.rank)
            if fullStats:
                print "\\",
                print " "*14, "({0:.6f}, {1:.6f})".format(cEntry.totalTimeSelf.getAvg(), cEntry.totalTimeSelf.getStdDev()).center(widthFloat),
                print " " + "({0:.6f}, {1:.6f})".format(cEntry.totalTimeInChildrenNoRecurse.getAvg(), cEntry.totalTimeInChildrenNoRecurse.getStdDev()).center(widthFloat),
                if doRecurse and cEntry.totalTimeRecurse <> 0:
                    print "[({0:.6f}, {1:.6f})]".format(cEntry.totalTimeRecurse.getAvg(), cEntry.totalTimeRecurse.getStdDev()).center(widthFloat+2),
                else:
                    print recurseEmptyField,
                print " " + "({0:.6f}, {1:.6f})".format(cEntry.count.getAvg(), cEntry.count.getStdDev()).center(widthInt) + \
                    "/" + "({0:.6f}, {1:.6f})".format(tlEntry.count.getAvg(), tlEntry.count.getStdDev()).center(widthInt)
        # Done for this function
        if fullStats:
            print "-"*220
        else:
            print "-"*110


### Main Program ###

class Usage:
    def __init__(self, msg):
        self.msg = msg

def main(argv=None):
    global funcNames
    global allEnsembles
    global allThreads
    if argv is None:
        argv = sys.argv

    lineEntry_re = re.compile(
        r"ENTRY ([0-9]+):([0-9]+) = count\(([0-9]+)\), sum\(([0-9.]+)\), sumSq\(([0-9.]+)\)(?:(?:$)|(?:, sumChild\(([0-9.]+)\), sumSqChild\(([0-9.]+)\), sumRecurse\(([0-9.]+)\), sumSqRecurse\(([0-9.]+)\)$))")
    lineDef_re = re.compile(r"DEF ([A-Za-z_0-9]+) ([0-9]+)")
    baseFile="profiler_"
    threads = range(0, 4)
    nodes=None
    doExtendedStats = False
    doRecurseStats = False
    baseFiles = []

    try:
        try:
            opts, args = getopt.getopt(argv[1:], "hb:t:n:sr", ["help", "base-file=", "thread=", "node=", "stats", "recurse"])
        except getopt.error, err:
            raise Usage(err)
        for o, a in opts:
            if o in ("-h", "--help"):
                raise Usage(\
"""
    -h,--help:      Prints this message
    -b,--base-file: Specifies the base name of the profile files (defaults to profiler_)
    -n,--node:      (optional) Specifies the nodes to use (if any):
                      - Can be 'start:end' where 'start' and 'end' are integers to specify
                        the files starting with '<base-file><start>-' to '<base-file><end-1>-'.
                        The thread information is used to complete the file names
                      - Can be a comma separated list of values
                      - It can also be '*' to look for all files beginning with '<base-file>'. Note that the
                        quotes around the * are important (otherwise, the shell will expand it itself)
                    Defaults to not specified
    -t,--thread:    Specifies the threads to use:
                      - Can be 'start:end' where 'start' and 'end' are integers to specify
                        the files '<base-file><node>-<start>' to '<base-file><node>-<end-1>'.
                      - Can be a comma separated list of values
                      - It can also be '*' to look for all files beginning with '<base-file><node>-'
                    Defaults to '0:4'
    -s,--stats:     (optional) If specified, will print out additional information
                    about the averages and standard deviations of values reported
                    for a node or for all threads.
    -r,--recurse:   (optional) If specified, will print out additional information about
                    recursive calls.
""")
            elif o in ("-b", "--base-file"):
                baseFile = a
            elif o in ("-t", "--thread"):
                if a == '*':
                    threads=[]
                elif ':' in a:
                    start,end = re.split(":", a)
                    threads = range(int(start), int(end))
                else:
                    threads = re.split(",", a)
            elif o in ("-n", "--node"):
                if a == '*':
                    nodes=[]
                elif ':' in a:
                    start,end = re.split(":", a)
                    nodes = range(int(start), int(end))
                else:
                    nodes = re.split(",", a)
            elif o in ("-s", "--stats"):
                doExtendedStats = True
            elif o in ("-r", "--recurse"):
                doRecurseStats = True
            else:
                raise Usage("Unhandled option")
        if args is not None and len(args) > 0:
            raise Usage("Extraneous arguments: %s" % (str(args)))
    except Usage, msg:
        print >>sys.stderr, msg.msg
        print >>sys.stderr, "For help use -h"
        return 2


    if nodes is None:
        baseFiles.append((baseFile, ''))
    else:
        if nodes == []:
            texp = re.compile('^%s([^-]*)-' % (baseFile))
            for file in os.listdir('.'):
                m = texp.match(file)
                if m is not None:
                    baseFiles.append((baseFile + m.group(1) + '-', m.group(1)))
            # end for
        else:
            baseFiles = [(baseFile + str(i) + '-', str(i)) for i in nodes]

    # Total ensemble (this will accumulate things across all threads
    allEnsembles['all'] = EnsembleStatistics('all', 'all')
    for (baseName, nodeName) in baseFiles:
        # We create an ensemble per baseName except if only one baseFiles
        if len(baseFiles) > 1:
            allEnsembles[baseName] = EnsembleStatistics(baseName, nodeName)
            ensemblesToUpdate = (allEnsembles['all'], allEnsembles[baseName])
        else:
            ensemblesToUpdate = (allEnsembles['all'],)

        allFiles = []
        if len(threads) == 0:
            texp = re.compile('^%s(.*)' % (baseName))
            for file in os.listdir('.'):
                m = texp.match(file)
                if m is not None:
                    allFiles.append((file, m.group(1)))
        else:
            allFiles = [(baseName + str(i), str(i)) for i in threads]

        for (fileName, threadName) in allFiles:
            print "Processing file %s..." % (fileName)
            myStats = ThreadStatistics(fileName, threadName)
            allThreads[fileName] = myStats

            fileHandle = open(fileName, "r")

            for line in fileHandle:
                matchOb = lineDef_re.match(line)
                if matchOb is not None:
                    funcName, funcId_s = matchOb.groups()
                    funcId = int(funcId_s)
                    if len(funcNames) <= funcId:
                        funcNames.extend(['']*(funcId-len(funcNames)+1))
                    funcNames[funcId] = funcName
                    continue
                # Here we should always have a match
                parentId_s, childId_s, count_s, sum_s, \
                    sumSq_s, sumChild_s, sumChildSq_s, \
                    sumRecurse_s, sumRecurseSq_s = lineEntry_re.match(line).groups()
                parentId = int(parentId_s)
                childId = int(childId_s)
                count = int(count_s)
                sum = float(sum_s)
                sumSq = float(sumSq_s)

                pFuncEntry = myStats.getFuncEntry(parentId, funcNames[parentId]) # Makes sure that we know about this function
                if parentId == childId:
                    # Self-node, we create a FuncEntry
                    assert(sumChild_s is None and sumChildSq_s is None)
                    pFuncEntry.set(count, sum, sumSq)
                else:
                    # Here we have a childEntry
                    assert(sumChild_s is not None and sumChildSq_s is not None)
                    child = ChildEntry(childId, funcNames[childId], myStats, count, sum,
                                       sumSq, float(sumChild_s), float(sumChildSq_s),
                                       float(sumRecurse_s), float(sumRecurseSq_s))
                    child.addParent(pFuncEntry)
                    pFuncEntry.addChild(child)

            fileHandle.close()
            for e in ensemblesToUpdate:
                e.addThreadStatistics(myStats)
    print "Done processing all files, computing all statistics..."

    for entry in allThreads.values():
        entry.calculateStatistics()
    for entry in allEnsembles.values():
        entry.calculateStatistics()

    onlyOneNode = len(allEnsembles) == 1
    print "Done computing statistics, sorting and printing..."
    for nodeBaseName, ensembleStat in iter(sorted(allEnsembles.iteritems(), key=itemgetter(0))):
        if nodeBaseName == 'all' and not onlyOneNode:
            continue # We print this at the very end
        if not onlyOneNode:
            print "############### Node %s ################" % (ensembleStat.nodeName)

        for threadStat in iter(sorted(ensembleStat.allThreads, key=itemgetter('threadName'))):
            # Calculate the total time for this thread
            totalMeasuredTime = 0.0
            for entry in threadStat.funcEntries.values():
                totalMeasuredTime += float(entry.totalTimeSelf)
            # Look at all the function and assign a rank
            sortedByTotal = sorted(threadStat.funcEntries.values(), key=itemgetter('totalTimeSelf'), reverse=True) # No iter since we go over it twice...
            for count, e in enumerate(sortedByTotal):
                e.rank = count+1 # Start at 1

            sortedEntries = iter(sorted(threadStat.funcEntries.values(), key=itemgetter('totalTime'), reverse=True))
            printThreadInfo("#### Thread %s ####" % (threadStat.threadName), totalMeasuredTime,
                            sortedByTotal, sortedEntries, False, doRecurseStats)
            print ""
        # End thread
        # We print the global statistics for the node
        if onlyOneNode:
            headerLine = "#### TOTAL ####"
        else:
            headerLine = "#### Node %s total ####" % (ensembleStat.nodeName)
        totalMeasuredTime = 0.0
        for entry in ensembleStat.gatheringThread.funcEntries.values():
            totalMeasuredTime += float(entry.totalTimeSelf)
        sortedByTotal = sorted(ensembleStat.gatheringThread.funcEntries.values(),
                               key=itemgetter('totalTimeSelf'), reverse=True)
        for count, e in enumerate(sortedByTotal):
            e.rank = count+1 # Start at 1
        sortedEntries = iter(sorted(ensembleStat.gatheringThread.funcEntries.values(),
                                    key=itemgetter('totalTime'), reverse=True))
        printThreadInfo(headerLine, totalMeasuredTime, sortedByTotal, sortedEntries, doExtendedStats, doRecurseStats)
        print ""
    # End node
    # If we have multiple nodes, print the total
    if not onlyOneNode:
        headerLine = "############### TOTAL ################"
        ensembleStat = allEnsembles['all']
        totalMeasuredTime = 0.0
        for entry in ensembleStat.gatheringThread.funcEntries.values():
            totalMeasuredTime += float(entry.totalTimeSelf)
        sortedByTotal = sorted(ensembleStat.gatheringThread.funcEntries.values(),
                               key=itemgetter('totalTimeSelf'), reverse=True)
        for count, e in enumerate(sortedByTotal):
            e.rank = count+1 # Start at 1
        sortedEntries = iter(sorted(ensembleStat.gatheringThread.funcEntries.values(),
                                    key=itemgetter('totalTime'), reverse=True))
        printThreadInfo(headerLine, totalMeasuredTime, sortedByTotal, sortedEntries, doExtendedStats, doRecurseStats)
    return 0

if __name__ == "__main__":
    sys.exit(main())


