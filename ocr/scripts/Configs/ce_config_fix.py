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
# This script modifies a few fields in the OCR config from an FSim config
# Currently it only changes core counts, future changes will involve memory

parser = argparse.ArgumentParser(description='Generate a modified OCR config \
        file for CE from original config file & FSim config file.')
parser.add_argument('--fsimcfg', dest='fsimcfg', default='config.cfg',
                   help='FSim config file to use (default: config.cfg)')
parser.add_argument('--ocrcfg', dest='ocrcfg', default='default.cfg',
                   help='OCR config file to use (will be overwritten)')

args = parser.parse_args()
fsimcfg = args.fsimcfg
ocrcfg = args.ocrcfg
neighbors = 0
xe_count = 0

def ExtractValues(infilename):
    global m1, m2, m4
    config = ConfigParser.SafeConfigParser(allow_no_value=True)
    config.readfp(infilename)
    # First, core counts
    cc = config.get('SocketGlobal', 'cluster_count').strip(' ').split(' ')[0]
    cc = int(''.join(itertools.takewhile(lambda s: s.isdigit(), cc)))
    bc = config.get('ClusterGlobal', 'block_count').strip(' ').split(' ')[0]
    bc = int(''.join(itertools.takewhile(lambda s: s.isdigit(), bc)))
    xc = config.get('BlockGlobal', 'xe_count').strip(' ').split(' ')[0]
    xc = int(''.join(itertools.takewhile(lambda s: s.isdigit(), xc)))

    # Next, memory sizes
    m1 = config.get('BlockGlobal', 'sl1_size').strip(' ').split(' ')[0]
    m1 = int(''.join(itertools.takewhile(lambda s: s.isdigit(), m1)))
    m1 = m1 * 1024
    m2 = config.get('BlockGlobal', 'sl2_size').strip(' ').split(' ')[0]
    m2 = int(''.join(itertools.takewhile(lambda s: s.isdigit(), m2)))
    m2 = m2 * 1024
    # m3 == sl3 which is omitted for now
    m4 = config.get('SocketGlobal', 'ipm_size').strip(' ').split(' ')[0]
    m4 = int(''.join(itertools.takewhile(lambda s: s.isdigit(), m4)))
    m4 = m4 * 1024 * 1024
    m5 = config.get('SocketGlobal', 'dram_size').strip(' ').split(' ')[0]
    m5 = int(''.join(itertools.takewhile(lambda s: s.isdigit(), m5)))
    m5 = m5 * 1024 * 1024
    m6 = config.get('SocketGlobal', 'nvm_size').strip(' ').split(' ')[0]
    m6 = int(''.join(itertools.takewhile(lambda s: s.isdigit(), m6)))
    m6 = m6 * 1024 * 1024

    global neighbors, xe_count
    neighbors = (bc-1)+(cc-1)
    xe_count = xc

def RewriteConfig(cfg):
    global neighbors, xe_count
    global m1, m2, m4
    with open(cfg, 'r+') as fp:
        lines = fp.readlines()
        fp.seek(0)
        fp.truncate()
        section = 0      # Keeps track of section being parsed
        for line in lines:
            if 'neighborcount' in line:
                line = '   neighborcount\t\t=\t'+ \
                    str(neighbors) + '\n'
            if 'neighbors' in line:
                if neighbors > 0:
                    line = '   neighbors\t\t=\t0-' \
                    + str(neighbors-1) + '\n'
                else:
                    line = '#neighbors\t\t=\n'
            if 'xecount' in line:
                line = '   xecount\t\t=\t' + \
                    str(xe_count) + '\n'

            # Ugly state-machine type logic to follow, can be cleaned up later if needed
            if 'MemPlatformInst0' in line:
                section = 1
            if 'MemPlatformInstForL2' in line:
                section = 2
            if 'MemPlatformInstForL4' in line:
                section = 4

            if 'MemTargetInst0' in line:
                section = 10
            if 'MemTargetInstForL2' in line:
                section = 20
            if 'MemTargetInstForL4' in line:
                section = 40

            if 'AllocatorInst0' in line:
                section = 100
            if 'AllocatorInstForL2' in line:
                section = 200
            if 'AllocatorInstForL4' in line:
                section = 400

            # Introduces issue #902
            if section == 1 and 'size' in line:
                # Sanity check
                value = int(line.split('=')[1].strip(), base=16)
                assert value < m1, "FSim config L1 size %lx smaller than OCR %lx" % (m1, value)
                m1 = m1 - 0x800      # issue #902
                section = 0

            if section == 2 and 'size' in line:
                line = '   size\t=\t' + hex(m2) + '\n'
                m2 = m2 - (m2 >> 5)  # issue #902
                section = 0

            if section == 4 and 'size' in line:
                line = '   size\t=\t' + hex(m4) + '\n'
                m4 = m4 - (m4 >> 5)
                section = 0

            if section == 10 and 'size' in line:
                # Sanity check
                value = int(line.split('=')[1].strip(), base=16)
                assert value < m1, "FSim config L1 size %lx smaller than OCR %lx" % (m1, value)
                m1 = m1 - 0x800
                section = 0

            if section == 20 and 'size' in line:
                line = '   size\t=\t' + hex(m2) + '\n'
                m2 = m2 - (m2 >> 5)
                section = 0

            if section == 40 and 'size' in line:
                line = '   size\t=\t' + hex(m4) + '\n'
                m4 = m4 - (m4 >> 5)
                section = 0

            if section == 100 and 'size' in line:
                # Sanity check
                value = int(line.split('=')[1].strip(), base=16)
                assert value < m1, "FSim config L1 size %lx smaller than OCR %lx" % (m1, value)
                section = 0

            if section == 200 and 'size' in line:
                line = '   size\t=\t' + hex(m2) + '\n'
                section = 0

            if section == 400 and 'size' in line:
                line = '   size\t=\t' + hex(m4) + '\n'
                section = 0

            fp.write(line)


def StripLeadingWhitespace(infile, outfile):
    with open(infile, "r") as inhandle:
        for line in inhandle:
            outfile.write(line.lstrip())

if os.path.isfile(ocrcfg):
    # Because python config parsing can't handle leading tabs
    with tempfile.TemporaryFile() as temphandle:
        StripLeadingWhitespace(fsimcfg, temphandle)
        temphandle.seek(0)
        ExtractValues(temphandle)
    RewriteConfig(ocrcfg)
else:
    print 'Unable to find OCR config file ', ocrcfg
    sys.exit(0)
