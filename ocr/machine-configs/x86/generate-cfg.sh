#!/bin/bash

export CFG_SCRIPT=../../scripts/Configs/config-generator.py
$CFG_SCRIPT --threads 8 --output jenkins-common-8w-lockableDB.cfg --remove-destination
$CFG_SCRIPT --threads 8 --dbtype Regular --output jenkins-common-8w-regularDB.cfg --remove-destination
$CFG_SCRIPT --threads 1 --dbtype Regular --output mach-hc-1w.cfg --remove-destination
$CFG_SCRIPT --threads 2 --dbtype Regular --output mach-hc-2w.cfg --remove-destination
$CFG_SCRIPT --threads 4 --dbtype Regular --output mach-hc-4w.cfg --remove-destination
$CFG_SCRIPT --threads 8 --dbtype Regular --output mach-hc-8w.cfg --remove-destination
$CFG_SCRIPT --threads 8 --dbtype Regular --binding spread --output mach-hc-8w-binding.cfg --remove-destination
$CFG_SCRIPT --threads 16 --dbtype Regular --output mach-hc-16w.cfg --remove-destination
$CFG_SCRIPT --threads 8 --scheduler STATIC --output static-8w-lockableDB.cfg --remove-destination
unset CFG_SCRIPT
