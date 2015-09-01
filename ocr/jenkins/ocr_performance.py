#!/usr/bin/env python

import os

jobtype_ocr_performance = {
    'name': 'ocr-performance',
    'isLocal': True,
    'run-cmd': '${JJOB_PRIVATE_HOME}/xstack/ocr/jenkins/scripts/performance-ubench.sh',
    'param-cmd': '${JJOB_PRIVATE_HOME}/xstack/ocr/jenkins/scripts/performance-ubench.sh _params',
    'keywords': ('ocr', 'performance'),
    'timeout': 240,
    'sandbox': ('local', 'shared', 'shareOK'),
    'req-repos': ('xstack',)
}

jobtype_gatherStats_performance = {
    'name': 'gatherStats-performance',
    'isLocal': True,
    'run-cmd': '${JJOB_SHARED_HOME}/xstack/ocr/jenkins/scripts/perfStatCollector.sh',
    'param-cmd': '${JJOB_SHARED_HOME}/xstack/jenkins/scripts/empty-cmd.sh',
    'keywords': ('ocr', 'performance'),
    'timeout': 60,
    'sandbox': ('shared', 'shareOK'),
    'req-repos': ('xstack',)
}

# Specific jobs

job_ocr_performance_x86_pthread_x86 = {
    'name': 'ocr-performance-x86',
    'depends': ('ocr-build-x86',),
    'jobtype': 'ocr-performance',
    'run-args': 'x86 jenkins-common-8w-regularDB.cfg regularDB',
    'sandbox': ('inherit0',)
}

#Aggregates execution times in csv file
job_ocr_performance_gatherStats = {
    'name': 'perfGatherStats',
    'depends': ('ocr-performance-x86',),
    'jobtype': 'gatherStats-performance',
    # This folder is where the 'ocr-performance' job puts results
    'run-args': '${JJOB_SHARED_HOME}/xstack/ocr/tests/performance-tests 10',
    'sandbox': ('shared','inherit0')
}
