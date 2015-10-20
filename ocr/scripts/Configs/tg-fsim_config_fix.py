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
# This script modifies the FSim configuration to:
# 1. Add common DVFS sections
# 2. Add the TG_INSTALL environment variable at the bottom so that
#    we don't have to change it in every configuration (if things change)

parser = argparse.ArgumentParser(description='Generate a modified FSim configuration \
        from an original.')
parser.add_argument('--tginstall', dest='tginstall', default='',
                   help='Path to the TG installation directory. (default: "")')
parser.add_argument('--fsimcfg', dest='fsimcfg', default='config.cfg',
                   help='FSim config file to use (will be overwritten). (default: config.cfg)')

args = parser.parse_args()
tginstall = args.tginstall
fsimcfg = args.fsimcfg

def RewriteConfig(cfg):
    dvfsConfig = tginstall + '/fsim-configs/dvfs-default.cfg'
    if os.path.isfile(dvfsConfig):
        with open(cfg, 'a+') as fp:
            with open(dvfsConfig, 'r') as dvfs:
                for l in dvfs:
                    fp.write(l)
            fp.write("""
[environment]
    TG_INSTALL = %s

""" % (tginstall))
    else:
        print 'Unable to find dvfs-default.cfg here: ', dvfsConfig

if os.path.isfile(fsimcfg):
    RewriteConfig(fsimcfg)
else:
    print 'Unable to find FSim config file ', fsimcfg
    sys.exit(0)
