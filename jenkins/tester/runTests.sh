#!/bin/bash

# Use this script to test what Jenkins would run on your current directory
# Launch this from the jenkins/tester/ directory

# By default, this will only run tests locally (no shared tests). Pass the
# keyword "shared" as the first argument if you would like to run all
# tests. Note that you MUST be on the XS cluster to run the shared tests

# By default, this will also only run tests that do not require the Intel
# private repository. Pass the path to the Intel checkout as the second
# argument if you want to run those tests. You will also need to pass
# an environment directory as the third argument

# Temporary directories are NOT cleaned up

# You can configure a few parameters (up to END_CONFIGURATION)

# Base directory where the private workspaces will be created
ROOT_BASE=/tmp
# Base directory where the shared workspaces will be created
SHARED_BASE=${HOME}
# Root directory for the GIT checkout
INIT_DIR=`pwd`/../../
# Directory where testrunner.py is located
TESTER_DIR=`pwd`
# Base configuration for the testrunner.py executable
TESTRUNNER_CONFIG="-t 4 --no-clean failure -r xstack:${INIT_DIR} -o runTestsResults.xml "

# END_CONFIGURATION

if [ $# > 0 ]; then
    SHARED=$1
else
    SHARED=""
fi

if [ $# > 2 ]; then
    TESTRUNNER_CONFIG="${TESTRUNNER_CONFIG} -r intel:$2 -e $3"
fi

ROOT_DIR=`mktemp -d --tmpdir=${ROOT_BASE}`
if [ "x$SHARED" == "xshared" ]; then
    SHARED_DIR=`mktemp -d --tmpdir=${SHARED_BASE}`
else
    SHARED_DIR=""
fi

PRIVATE_DIR=${ROOT_DIR}/private
mkdir ${PRIVATE_DIR}

echo "Created root directory ${ROOT_DIR} and shared directory ${SHARED_DIR} (none if empty)"

TESTRUNNER_CONFIG="-c ${TESTER_DIR}/../all ${TESTRUNNER_CONFIG} -p ${PRIVATE_DIR}"
if [ "x$SHARED" == "xshared" ]; then
    TESTRUNNER_CONFIG="${TESTRUNNER_CONFIG} -s ${SHARED_DIR}"
else
    TESTRUNNER_CONFIG="${TESTRUNNER_CONFIG} --local-only"
fi

${TESTER_DIR}/testrunner.py ${TESTRUNNER_CONFIG}

