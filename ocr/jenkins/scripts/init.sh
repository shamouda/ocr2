#!/bin/bash

# We start out in JJOB_PRIVATE_HOME

if [ $# > 0 ]; then
    TG=$1
else
    TG=""
fi

# Make sure the Jenkins system is fully accessible in the shared home
# We also copy any apps/ makefiles and datasets
# We also copy the CnC-OCR toolchain
mkdir -p ${JJOB_SHARED_HOME}/xstack/jenkins
mkdir -p ${JJOB_SHARED_HOME}/xstack/ocr/jenkins
mkdir -p ${JJOB_SHARED_HOME}/xstack/ocr/scripts
mkdir -p ${JJOB_SHARED_HOME}/xstack/ocr/examples
mkdir -p ${JJOB_SHARED_HOME}/xstack/apps
mkdir -p ${JJOB_SHARED_HOME}/xstack/hll/cnc

cp -r ${JJOB_PRIVATE_HOME}/xstack/jenkins/* ${JJOB_SHARED_HOME}/xstack/jenkins/
cp -r ${JJOB_PRIVATE_HOME}/xstack/ocr/jenkins/* ${JJOB_SHARED_HOME}/xstack/ocr/jenkins/
cp -r ${JJOB_PRIVATE_HOME}/xstack/ocr/scripts/* ${JJOB_SHARED_HOME}/xstack/ocr/scripts/
cp -r ${JJOB_PRIVATE_HOME}/xstack/ocr/examples/* ${JJOB_SHARED_HOME}/xstack/ocr/examples/
rsync -aq -r ${JJOB_PRIVATE_HOME}/xstack/apps/ ${JJOB_SHARED_HOME}/xstack/apps/ --exclude libs
mkdir -p ${JJOB_SHARED_HOME}/xstack/apps/libs
cp -r ${JJOB_PRIVATE_HOME}/xstack/apps/libs/src ${JJOB_SHARED_HOME}/xstack/apps/libs/src
cp -rT ${JJOB_PRIVATE_HOME}/xstack/hll/cnc ${JJOB_SHARED_HOME}/xstack/hll/cnc

if [ "x$TG" == "xtg" ]; then
    mkdir -p ${JJOB_SHARED_HOME}/intel/ss/jenkins
    cp -r ${JJOB_PRIVATE_HOME}/intel/ss/jenkins/* ${JJOB_SHARED_HOME}/intel/ss/jenkins/
    mkdir -p ${JJOB_SHARED_HOME}/xstack/apps/libs/tg/lib
    mkdir -p ${JJOB_SHARED_HOME}/xstack/apps/libs/tg/include
fi

mkdir -p ${JJOB_SHARED_HOME}/xstack/apps/libs/x86/lib
mkdir -p ${JJOB_SHARED_HOME}/xstack/apps/libs/x86/include
ln -s ${JJOB_SHARED_HOME}/xstack/apps/makefiles ${JJOB_SHARED_HOME}/xstack/ocr/examples/makefiles
