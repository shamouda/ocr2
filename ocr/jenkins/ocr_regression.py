#!/usr/bin/env python

import os

# Make a copy of the local home because the regressions run
# in place for now
jobtype_ocr_regression = {
    'name': 'ocr-regression',
    'isLocal': True,
    'run-cmd': '${JJOB_INITDIR_OCR}/ocr/jenkins/scripts/regression.sh',
    'param-cmd': '${JJOB_INITDIR_OCR}/ocr/jenkins/scripts/regression.sh _params',
    'keywords': ('ocr', 'regression', 'percommit'),
    'timeout': 240,
    'sandbox': ('local', 'shared', 'shareOK'),
    'req-repos': ('ocr',)
}

# Specific jobs

job_ocr_regression_x86_pthread_x86 = {
    'name': 'ocr-regression-x86',
    'depends': ('ocr-build-x86',),
    'jobtype': 'ocr-regression',
    'run-args': 'x86 jenkins-common-8w-regularDB.cfg regularDB',
    'sandbox': ('inherit0',)
}

job_ocr_regression_x86_pthread_x86_lockableDB = {
    'name': 'ocr-regression-x86-lockableDB',
    'depends': ('ocr-build-x86',),
    'jobtype': 'ocr-regression',
    'run-args': 'x86 jenkins-common-8w-lockableDB.cfg lockableDB',
    'sandbox': ('inherit0',)
}

job_ocr_regression_x86_pthread_tg_regularDB = {
    'name': 'ocr-regression-tg-x86-regularDB',
    'depends': ('ocr-build-tg-x86',),
    'jobtype': 'ocr-regression',
    'run-args': 'tg-x86 jenkins-1block-regularDB.cfg regularDB',
    'timeout': 400,
    'sandbox': ('inherit0',),
    'env-vars': { 'TG_INSTALL': '${JJOB_ENVDIR}' }
}

job_ocr_regression_x86_pthread_tg_lockableDB = {
    'name': 'ocr-regression-tg-x86-lockableDB',
    'depends': ('ocr-build-tg-x86',),
    'jobtype': 'ocr-regression',
    'run-args': 'tg-x86 jenkins-1block-lockableDB.cfg lockableDB',
    'timeout': 400,
    'sandbox': ('inherit0',),
    'env-vars': { 'TG_INSTALL': '${JJOB_ENVDIR}' }
}

#TODO: not sure how to not hardcode MPI_ROOT here
job_ocr_regression_x86_pthread_mpi_lockableDB = {
    'name': 'ocr-regression-x86-mpi-lockableDB',
    'depends': ('ocr-build-x86-mpi',),
    'jobtype': 'ocr-regression',
    'run-args': 'x86-mpi jenkins-x86-mpi.cfg lockableDB',
    'sandbox': ('inherit0',),
    'env-vars': {'MPI_ROOT': '/opt/intel/tools/impi/5.1.1.109/intel64',
                 'PATH': '${MPI_ROOT}/bin:'+os.environ['PATH'],
                 'LD_LIBRARY_PATH': '${MPI_ROOT}/lib64',}
}

#TODO: not sure how to not hardcode GASNET_ROOT here
job_ocr_regression_x86_pthread_gasnet_lockableDB = {
    'name': 'ocr-regression-x86-gasnet-lockableDB',
    'depends': ('ocr-build-x86-gasnet',),
    'jobtype': 'ocr-regression',
    'run-args': 'x86-gasnet jenkins-x86-gasnet.cfg lockableDB',
    'sandbox': ('inherit0',),
    'env-vars': {'MPI_ROOT': '/opt/intel/tools/impi/5.1.1.109/intel64',
                 'GASNET_ROOT': '/opt/rice/GASNet/1.24.0-impi',
                 'PATH': '${GASNET_ROOT}/bin:${MPI_ROOT}/bin:'+os.environ['PATH'],
                 'GASNET_CONDUIT': 'ibv',
                 'GASNET_TYPE': 'par',
                 'GASNET_EXTRA_LIBS': '-L/usr/lib64 -lrt -libverbs',
                 'CC': 'mpicc', # gasnet built with mpi
                 # picked up by non-regression test script
                 'OCR_LDFLAGS': '-L${GASNET_ROOT}/lib -lgasnet-${GASNET_CONDUIT}-${GASNET_TYPE} ${GASNET_EXTRA_LIBS}',}
}
