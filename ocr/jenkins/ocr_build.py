#!/usr/bin/env python

import os

jobtype_ocr_init = {
    'name': 'ocr-init',
    'isLocal': True,
    'run-cmd': '${JJOB_PRIVATE_HOME}/xstack/ocr/jenkins/scripts/init.sh',
    'param-cmd': '${JJOB_PRIVATE_HOME}/xstack/jenkins/scripts/empty-cmd.sh',
    'keywords': ('ocr', 'percommit'),
    'timeout': 160,
    'sandbox': ('local', 'shared', 'emptyShared', 'shareOK'),
    'req-repos': ('xstack',)
}

# Note that we could do away with the copy entirely and just
# copy the build directory but keeping for now
jobtype_ocr_build = {
    'name': 'ocr-build',
    'isLocal': True,
    'run-cmd': '${JJOB_PRIVATE_HOME}/xstack/ocr/jenkins/scripts/build.sh',
    'param-cmd': '${JJOB_PRIVATE_HOME}/xstack/jenkins/scripts/empty-cmd.sh',
    'keywords': ('ocr', 'percommit'),
    'timeout': 120,
    'sandbox': ('local', 'shared', 'shareOK'),
    'req-repos': ('xstack',),
    'env-vars': {'OCR_ROOT': '${JJOB_PRIVATE_HOME}/xstack/ocr',
                 'OCR_BUILD_ROOT': '${JJOB_PRIVATE_HOME}/xstack/ocr/build',
                 'OCR_INSTALL_ROOT': '${JJOB_SHARED_HOME}/xstack/ocr/install'}
}

jobtype_ocr_build_tg = {
    'name': 'ocr-build-tg',
    'isLocal': True,
    'run-cmd': '${JJOB_PRIVATE_HOME}/xstack/ocr/jenkins/scripts/build.sh',
    'param-cmd': '${JJOB_PRIVATE_HOME}/xstack/jenkins/scripts/empty-cmd.sh',
    'keywords': ('ocr', 'percommit'),
    'timeout': 120,
    'sandbox': ('local', 'shared', 'shareOK'),
    'req-repos': ('xstack', 'intel'),
    'env-vars': {'TG_INSTALL': '${JJOB_ENVDIR}',
                 'TG_ROOT': '${JJOB_PRIVATE_HOME}/intel/ss',
                 'OCR_ROOT': '${JJOB_PRIVATE_HOME}/xstack/ocr',
                 'OCR_BUILD_ROOT': '${JJOB_PRIVATE_HOME}/xstack/ocr/build',
                 'OCR_INSTALL_ROOT': '${JJOB_SHARED_HOME}/xstack/ocr/install'}
}

# Specific jobs

pick_one_of_ocr_init = {
    'name': 'ocr-init',
    'alternates': ( 'ocr-init-job', 'ocr-init-job-tg' )
}

job_ocr_init = {
    'name': 'ocr-init-job',
    'depends': (),
    'jobtype': 'ocr-init',
    'run-args': ''
}

job_ocr_init_tg = {
    'name': 'ocr-init-job-tg',
    'depends': ('ss-build-check-env',),
    'jobtype': 'ocr-init',
    'run-args': 'tg',
    'req-repos': ('intel',)
}

job_ocr_build_x86_pthread_x86 = {
    'name': 'ocr-build-x86-pthread-x86',
    'depends': ('__alternate ocr-init',),
    'jobtype': 'ocr-build',
    'run-args': 'x86-pthread-x86',
    'sandbox': ('inherit0',)
}

#TODO: not sure how to not hardcode MPI_ROOT here
job_ocr_build_x86_pthread_mpi = {
    'name': 'ocr-build-x86-pthread-mpi',
    'depends': ('__alternate ocr-init',),
    'jobtype': 'ocr-build',
    'run-args': 'x86-pthread-mpi',
    'sandbox': ('inherit0',),
    'env-vars': {'MPI_ROOT': '/opt/intel/tools/impi/5.0.1.035',
                 'PATH': '${MPI_ROOT}/bin64:'+os.environ['PATH'],}
}

job_ocr_build_x86_pthread_tg = {
    'name': 'ocr-build-x86-pthread-tg',
    'depends': ('__alternate ocr-init',),
    'jobtype': 'ocr-build',
    'run-args': 'x86-pthread-tg',
    'sandbox': ('inherit0',)
}

job_ocr_build_x86_builder_tg_ce = {
    'name': 'ocr-build-x86-builder-tg-ce',
    'depends': ('__alternate ocr-init',),
    'jobtype': 'ocr-build-tg',
    'run-args': 'x86-builder-tg-ce',
    'sandbox': ('inherit0',)
}

job_ocr_build_x86_builder_tg_xe = {
    'name': 'ocr-build-x86-builder-tg-xe',
    'depends': ('__alternate ocr-init',),
    'jobtype': 'ocr-build-tg',
    'run-args': 'x86-builder-tg-xe',
    'sandbox': ('inherit0',)
}

job_ocr_build_tg_null_tg_ce = {
    'name': 'ocr-build-tg-null-tg-ce',
    'depends': ('__alternate ocr-init',),
    'jobtype': 'ocr-build-tg',
    'run-args': 'tg-null-tg-ce',
    'sandbox': ('inherit0',)
}

job_ocr_build_tg_null_tg_xe = {
    'name': 'ocr-build-tg-null-tg-xe',
    'depends': ('__alternate ocr-init',),
    'jobtype': 'ocr-build-tg',
    'run-args': 'tg-null-tg-xe',
    'sandbox': ('inherit0',)
}

