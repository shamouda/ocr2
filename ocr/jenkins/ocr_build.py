#!/usr/bin/env python

import os

jobtype_ocr_init = {
    'name': 'ocr-init',
    'isLocal': True,
    'run-cmd': '${JJOB_PRIVATE_HOME}/xstack/ocr/jenkins/scripts/init.sh',
    'param-cmd': '${JJOB_PRIVATE_HOME}/xstack/jenkins/scripts/empty-cmd.sh',
    'keywords': ('ocr', 'percommit'),
    'timeout': 300,
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
    'timeout': 240,
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
    'timeout': 180,
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
    'name': 'ocr-build-x86',
    'depends': ('__alternate ocr-init',),
    'jobtype': 'ocr-build',
    'run-args': 'x86',
    'sandbox': ('inherit0',)
}

#TODO: not sure how to not hardcode MPI_ROOT here
job_ocr_build_x86_pthread_mpi = {
    'name': 'ocr-build-x86-mpi',
    'depends': ('__alternate ocr-init',),
    'jobtype': 'ocr-build',
    'run-args': 'x86-mpi',
    'sandbox': ('inherit0',),
    'env-vars': {'MPI_ROOT': '/opt/intel/tools/impi/5.1.1.109/intel64',
                 'PATH': '${MPI_ROOT}/bin:'+os.environ['PATH'],}
}

job_ocr_build_x86_pthread_gasnet = {
    'name': 'ocr-build-x86-gasnet',
    'depends': ('__alternate ocr-init',),
    'jobtype': 'ocr-build',
    'run-args': 'x86-gasnet',
    'sandbox': ('inherit0',),
    'env-vars': {'GASNET_ROOT': '/opt/rice/GASNet/1.24.0-impi',
                 'PATH': '${GASNET_ROOT}/bin:'+os.environ['PATH'],
                 'GASNET_CONDUIT': 'ibv',
                 'GASNET_TYPE': 'par'}
}

# job_ocr_build_x86_pthread_tg = {
#     'name': 'ocr-build-tg-x86',
#     'depends': ('__alternate ocr-init',),
#     'jobtype': 'ocr-build',
#     'run-args': 'tg-x86',
#     'sandbox': ('inherit0',)
# }

job_ocr_build_x86_builder_tg_ce = {
    'name': 'ocr-build-builder-ce',
    'depends': ('__alternate ocr-init',),
    'jobtype': 'ocr-build-tg',
    'run-args': 'builder-ce',
    'sandbox': ('inherit0',)
}

job_ocr_build_x86_builder_tg_xe = {
    'name': 'ocr-build-builder-xe',
    'depends': ('__alternate ocr-init',),
    'jobtype': 'ocr-build-tg',
    'run-args': 'builder-xe',
    'sandbox': ('inherit0',)
}

job_ocr_build_tg_null_tg_ce = {
    'name': 'ocr-build-tg-ce',
    'depends': ('__alternate ocr-init',),
    'jobtype': 'ocr-build-tg',
    'run-args': 'tg-ce',
    'sandbox': ('inherit0',)
}

job_ocr_build_tg_null_tg_xe = {
    'name': 'ocr-build-tg-xe',
    'depends': ('__alternate ocr-init',),
    'jobtype': 'ocr-build-tg',
    'run-args': 'tg-xe',
    'sandbox': ('inherit0',)
}
