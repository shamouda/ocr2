#!/usr/bin/python

import argparse
import multiprocessing
import os.path
import sys
import ConfigParser
import io
import itertools
import tempfile

# README First
# This script modifies a few fields in the OCR config file:
# 1. Modifies L1 SPAD start address based on how much of it is already being used by ELF sections
# 2. Modifies DRAM start address based no how much of it is being used by ELF section

parser = argparse.ArgumentParser(description='Generate a modified OCR config \
        file for XE from original config file & binary file.')
parser.add_argument('--binstart', dest='binstart', default='0x0',
                   help='Binary file start address (default: 0x0)')
parser.add_argument('--binend', dest='binend', default='0x0',
                   help='Binary file end address (default: 0x0)')
parser.add_argument('--ocrcfg', dest='ocrcfg', default='default.cfg',
                   help='OCR config file to use (will be overwritten)')

args = parser.parse_args()
binstart = args.binstart
binend = args.binend
binsize = long(binend, 16) - long(binstart, 16)
ocrcfg = args.ocrcfg
print "Size is 0x%lx" % (binsize,)

def ExtractValues(infilename):
    config = ConfigParser.SafeConfigParser(allow_no_value=True)
    config.readfp(infilename)
    global platstart, platsize, tgtsize, allocsize
    # FIXME: Currently only does for L1 scratchpad - the below ini strings are hardcoded
    platstart = config.get('MemPlatformInst0', 'start').strip(' ').split(' ')[0]
    platstart = ''.join(itertools.takewhile(lambda s: s.isalnum(), platstart))
    platsize = config.get('MemPlatformInst0', 'size').strip(' ').split(' ')[0]
    platsize = ''.join(itertools.takewhile(lambda s: s.isalnum(), platsize))
    tgtsize = config.get('MemTargetInst0', 'size').strip(' ').split(' ')[0]
    tgtsize = ''.join(itertools.takewhile(lambda s: s.isalnum(), tgtsize))
    allocsize = config.get('AllocatorInst0', 'size').strip(' ').split(' ')[0]
    allocsize = ''.join(itertools.takewhile(lambda s: s.isalnum(), allocsize))

def RewriteConfig(cfg):
    global platsize, tgtsize, allocsize

    with open(cfg, 'r+') as fp:
        lines = fp.readlines()
        fp.seek(0)
        fp.truncate()
        section = 0    # Keeps track of section being parsed
        for line in lines:
            if 'MemPlatformInst0' in line:
                section = 1
            if 'MemTargetInst0' in line:
                section = 2
            if 'AllocatorInst0' in line:
                section = 3
            if section == 1 and 'start' in line:
                line = '   start = \t' + hex(long(platstart,16)+binsize) + '\n'
            if section == 1 and 'size' in line:
                line = '   size =\t' + hex(long(platsize,16)-binsize) + '\n'
                section = 0
            if section == 2 and 'size' in line:
                line = '   size =\t' + hex(long(tgtsize,16)-binsize) + '\n'
                section = 0
            if section == 3 and 'size' in line:
                line = '   size =\t' + hex(long(allocsize,16)-binsize) + '\n'
                section = 0

            fp.write(line)


def StripLeadingWhitespace(infile, outfile):
    with open(infile, "r") as inhandle:
        for line in inhandle:
            outfile.write(line.lstrip())

if os.path.isfile(ocrcfg):
    # Because python config parsing can't handle leading tabs
    with tempfile.TemporaryFile() as temphandle:
        StripLeadingWhitespace(ocrcfg, temphandle)
        temphandle.seek(0)
        ExtractValues(temphandle)
    RewriteConfig(ocrcfg)
else:
    print 'Unable to find OCR config file ', ocrcfg
    sys.exit(0)
