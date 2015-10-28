#!/usr/bin/python

import fileinput
import argparse
import os
import sys
import ConfigParser

# This script cleans up a configuration file. It does this by
# removing all earlier entries of each section, whenever a section is duplicated.

parser = argparse.ArgumentParser(description='Combine several config files into a \
            single one by removing earlier entries of duplicated sections in an OCR \
            config file.')
parser.add_argument('--file', dest='output', required=True,
                   help='config file to append to')
parser.add_argument('files', metavar='<other config files>', type=str, nargs='+',
                   help='config files to append')

# Parse args

args = parser.parse_args()
filename = args.output
inputs = args.files

# Get rid of whitespace to keep ConfigParser happy

for line in fileinput.input(filename, inplace=True):
    print line.strip()

for f in inputs:
    for line in fileinput.input(f, inplace=True):
        print line.strip()

# Do the following in a loop
# For each file f provided for appending:
#  Remove all sections in config file that are duplicated in f
#  Append f to config file

config = ConfigParser.RawConfigParser()
config.read(filename)

for f in inputs:
    appendfile = ConfigParser.RawConfigParser()
    appendfile.read(f)
    sections = appendfile.sections()
    for sec in sections:
        if config.has_section(sec):
            config.remove_section(sec)
        config.add_section(sec)
        for opt in appendfile.options(sec):
            config.set(sec, opt, appendfile.get(sec,opt))

with open(filename, 'wb') as cfgfile:
    config.write(cfgfile)
