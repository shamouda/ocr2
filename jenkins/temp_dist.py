#!/usr/bin/env python

jobtype_temp_dist = {
    'name': 'temp-dist',
    'isLocal': False,
    'run-cmd': '${JJOB_START_HOME}/jenkins/scripts/testdist.sh',
    'param-cmd': '${JJOB_START_HOME}/jenkins/scripts/testdistparams.sh',
    'keywords': (),
    'timeout': 120,
    'sandbox': ('shared',)
}

job_temp_dist_job = {
    'name': 'temp-dist-job',
    'depends': (),
    'jobtype': 'temp-dist',
    'run-args': '1 2 3'
}
