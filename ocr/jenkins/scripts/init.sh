#!/bin/bash

# We start out in JJOB_PRIVATE_HOME

if [ $# > 0 ]; then
    TG=$1
else
    TG=""
fi

# Make sure the Jenkins system is fully accessible in the shared home
# We also copy any apps/ makefiles and datasets
mkdir -p ${JJOB_SHARED_HOME}/xstack/jenkins
mkdir -p ${JJOB_SHARED_HOME}/xstack/ocr/jenkins
mkdir -p ${JJOB_SHARED_HOME}/xstack/ocr/scripts
mkdir -p ${JJOB_SHARED_HOME}/xstack/ocr/ocr-apps
mkdir -p ${JJOB_SHARED_HOME}/xstack/apps

cp -r ${JJOB_PRIVATE_HOME}/xstack/jenkins/* ${JJOB_SHARED_HOME}/xstack/jenkins/
cp -r ${JJOB_PRIVATE_HOME}/xstack/ocr/jenkins/* ${JJOB_SHARED_HOME}/xstack/ocr/jenkins/
cp -r ${JJOB_PRIVATE_HOME}/xstack/ocr/scripts/* ${JJOB_SHARED_HOME}/xstack/ocr/scripts/
cp -r ${JJOB_PRIVATE_HOME}/xstack/ocr/ocr-apps/* ${JJOB_SHARED_HOME}/xstack/ocr/ocr-apps/
rsync -av -r ${JJOB_PRIVATE_HOME}/apps/ ${JJOB_SHARED_HOME}/apps/ --exclude libs --exclude makefiles

if [ "x$TG" == "xtg" ]; then
    mkdir -p ${JJOB_SHARED_HOME}/intel/ss/jenkins
    cp -r ${JJOB_PRIVATE_HOME}/intel/ss/jenkins/* ${JJOB_SHARED_HOME}/intel/ss/jenkins/
    mkdir -p ${JJOB_SHARED_HOME}/xstack/apps/libs/tg/lib
    mkdir -p ${JJOB_SHARED_HOME}/xstack/apps/libs/tg/include
fi

mkdir -p ${JJOB_SHARED_HOME}/xstack/apps/libs/x86/lib
mkdir -p ${JJOB_SHARED_HOME}/xstack/apps/libs/x86/include
ln -s ${JJOB_SHARED_HOME}/xstack/ocr/ocr-apps/makefiles ${JJOB_SHARED_HOME}/xstack/apps/makefiles
