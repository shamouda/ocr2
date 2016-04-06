#!/bin/bash

# We start out in JJOB_PRIVATE_HOME

if [ $# > 0 ]; then
    TG=$1
else
    TG=""
fi

# Make sure the Jenkins system is fully accessible in the shared home
mkdir -p ${JJOB_SHARED_HOME}/ocr/jenkins
mkdir -p ${JJOB_SHARED_HOME}/ocr/ocr/jenkins
mkdir -p ${JJOB_SHARED_HOME}/ocr/ocr/scripts

cp -r ${JJOB_PRIVATE_HOME}/ocr/jenkins/* ${JJOB_SHARED_HOME}/ocr/jenkins/
cp -r ${JJOB_PRIVATE_HOME}/ocr/ocr/jenkins/* ${JJOB_SHARED_HOME}/ocr/ocr/jenkins/
cp -r ${JJOB_PRIVATE_HOME}/ocr/ocr/scripts/* ${JJOB_SHARED_HOME}/ocr/ocr/scripts/

if [ "x$TG" == "xtg" ]; then
    mkdir -p ${JJOB_SHARED_HOME}/tg/tg/jenkins
    cp -r ${JJOB_PRIVATE_HOME}/tg/tg/jenkins/* ${JJOB_SHARED_HOME}/tg/tg/jenkins/
fi
