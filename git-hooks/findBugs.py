#!/usr/bin/env python

import fileinput, re

plusLine = re.compile(u"^\+ .*BUG\s+#([0-9]+).*", re.IGNORECASE)
minusLine = re.compile(u"^- .*BUG\s+#([0-9]+).*", re.IGNORECASE)
sepLine = re.compile(u"^@@.*")

fixedBugs = set()
newBugs = set()

plusBugs = []
minusBugs = []

def updateBugs(plus, minus):
    global fixedBugs, newBugs
    tplus = frozenset(plus)
    tminus = frozenset(minus)
    if len(tplus) or len(tminus):
        fixed = tminus - tplus
        new = tplus - tminus
        fixedBugs.update(fixed)
        newBugs.update(new)

for line in fileinput.input():
    matchObj = plusLine.match(line)
    if matchObj is not None:
        plusBugs.append(int(matchObj.group(1)))
    matchObj = minusLine.match(line)
    if matchObj is not None:
        minusBugs.append(int(matchObj.group(1)))
    matchObj = sepLine.match(line)
    if matchObj is not None:
        updateBugs(plusBugs, minusBugs)
        plusBugs = []
        minusBugs =[]
updateBugs(plusBugs, minusBugs)

if len(fixedBugs) or len(newBugs):
    print "# This commit seems to modify the following bugs. Please ensure"
    print "# accuracy and add to your commit message (uncomment)"
if len(fixedBugs):
    print "# Fixes %s" % (', '.join(["#%d" % (i) for i in fixedBugs]))
if len(newBugs):
    print "# See %s" % (', '.join(["#%d" % (i) for i in newBugs]))
