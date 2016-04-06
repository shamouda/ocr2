#!/usr/bin/env python

import os

jobtype_ocr_performance = {
    'name': 'ocr-performance',
    'isLocal': True,
    'run-cmd': '${JJOB_INITDIR_OCR}/ocr/jenkins/scripts/performance-ubench.sh',
    'param-cmd': '${JJOB_INITDIR_OCR}/ocr/jenkins/scripts/performance-ubench.sh _params',
    'keywords': ('ocr', 'performance', 'nightly'),
    'timeout': 600,
    'sandbox': ('local', 'shared', 'shareOK'),
    'req-repos': ('ocr',)
}

jobtype_gatherStats_performance = {
    'name': 'gatherStats-performance',
    'isLocal': True,
    'run-cmd': '${JJOB_INITDIR_OCR}/ocr/jenkins/scripts/perfStatCollector.sh',
    'param-cmd': '${JJOB_INITDIR_OCR}/jenkins/scripts/empty-cmd.sh',
    'keywords': ('ocr', 'performance', 'nightly'),
    'timeout': 60,
    'sandbox': ('shared', 'shareOK'),
    'req-repos': ('ocr',)
}

# Specific jobs

job_ocr_performance_x86_pthread_x86 = {
    'name': 'ocr-performance-x86',
    'depends': ('ocr-build-x86-perfs',),
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
    'run-args': '${JJOB_SHARED_HOME}/ocr/ocr/tests/performance-tests 10',
    'sandbox': ('shared','inherit0')
}
