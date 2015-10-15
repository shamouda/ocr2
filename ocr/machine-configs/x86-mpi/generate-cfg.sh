#!/bin/bash

export CFG_SCRIPT=../../scripts/Configs/config-generator.py
$CFG_SCRIPT --guid COUNTED_MAP --target mpi --threads 8 --output jenkins-hc-dist-mpi-clone-8w-lockableDB.cfg --remove-destination
$CFG_SCRIPT --guid COUNTED_MAP --target mpi --threads 2 --output mach-hc-dist-mpi-clone-2w-lockableDB.cfg --remove-destination
$CFG_SCRIPT --guid COUNTED_MAP --target mpi --threads 4 --output mach-hc-dist-mpi-clone-4w-lockableDB.cfg --remove-destination
$CFG_SCRIPT --guid COUNTED_MAP --target mpi --threads 8 --output mach-hc-dist-mpi-clone-8w-lockableDB.cfg --remove-destination
unset CFG_SCRIPT
