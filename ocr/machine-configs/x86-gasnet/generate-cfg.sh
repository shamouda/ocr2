#!/bin/bash

export CFG_SCRIPT=../../scripts/Configs/config-generator.py
$CFG_SCRIPT --guid COUNTED_MAP --target gasnet --threads 8 --output jenkins-hc-dist-gasnet-clone-8w-lockableDB.cfg --remove-destination
$CFG_SCRIPT --guid COUNTED_MAP --target gasnet --threads 2 --output mach-hc-dist-gasnet-clone-2w.cfg --remove-destination
$CFG_SCRIPT --guid COUNTED_MAP --target gasnet --threads 4 --output mach-hc-dist-gasnet-clone-4w.cfg --remove-destination
$CFG_SCRIPT --guid COUNTED_MAP --target gasnet --threads 8 --output mach-hc-dist-gasnet-clone-8w.cfg --remove-destination
$CFG_SCRIPT --guid COUNTED_MAP --target gasnet --threads 16 --output mach-hc-dist-gasnet-clone-16w.cfg --remove-destination
unset CFG_SCRIPT
