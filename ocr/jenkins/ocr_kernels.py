#!/usr/bin/env python

# This relies on stuff (OCR) being built in the shared
# home but does not need any separate checkout as it
# runs completely out of the source directory
jobtype_ocr_build_kernel = {
    'name': 'ocr-build-kernel',
    'isLocal': True,
    'run-cmd': '${JJOB_SHARED_HOME}/ocr/jenkins/scripts/kernel-build.sh',
    'param-cmd': '${JJOB_SHARED_HOME}/jenkins/scripts/empty-cmd.sh',
    'keywords': ('ocr', 'percommit'),
    'timeout': 300,
    'sandbox': ('local', 'shared', 'shareOK'),
    'env-vars': { 'TG_INSTALL': '${JJOB_ENVDIR}',
                  'TG_SRC': '${JJOB_INITDIR}/ss',
                  'APPS_ROOT': '${JJOB_INITDIR}/apps',
                  'OCR_SRC': '${JJOB_PRIVATE_HOME}/ocr',
                  'OCR_INSTALL_ROOT': '${JJOB_SHARED_HOME}/ocr/install',
                  'OCR_BUILD_ROOT': '${JJOB_PRIVATE_HOME}/ocr/build'}
}

jobtype_ocr_run_kernel_local = {
    'name': 'ocr-run-kernel-local',
    'isLocal': True,
    'run-cmd': '${JJOB_SHARED_HOME}/ocr/jenkins/scripts/kernel-run-local.sh',
    'param-cmd': '${JJOB_SHARED_HOME}/jenkins/scripts/empty-cmd.sh',
    'keywords': ('ocr', 'percommit'),
    'timeout': 300,
    'sandbox': ('shared', 'shareOK'),
    'env-vars': { 'TG_INSTALL': '${JJOB_ENVDIR}',
                  'APPS_ROOT': '${JJOB_INITDIR}/apps',
                  'OCR_BUILD_ROOT': '${JJOB_PARENT_PRIVATE_HOME_0}/ocr/build',
                  'OCR_INSTALL_ROOT': '${JJOB_SHARED_HOME}/ocr/install'}
}

# FIXME: Make this use the make run...
jobtype_ocr_run_kernel_remote = {
    'name': 'ocr-run-kernel-remote',
    'isLocal': False,
    'run-cmd': '${JJOB_SHARED_HOME}/ocr/jenkins/scripts/kernel-run-remote.sh',
    'param-cmd': '${JJOB_SHARED_HOME}/ss/jenkins/scripts/fsim-param-cmd.sh',
    'epilogue-cmd': '${JJOB_ENVDIR}/bin/fsim-scripts/fsim-epilogue.sh',
    'keywords': ('ocr', 'percommit'),
    'timeout': 300,
    'sandbox': ('shared', 'shareOK'),
    'env-vars': { 'TG_INSTALL': '${JJOB_ENVDIR}',
                  'APPS_ROOT': '${JJOB_INITDIR}/apps',
                  'OCR_INSTALL_ROOT': '${JJOB_SHARED_HOME}/ocr/install'}
}

jobtype_ocr_verify_kernel_local = {
    'name': 'ocr-verify-kernel-local',
    'isLocal': True,
    'run-cmd': '${JJOB_SHARED_HOME}/ocr/jenkins/scripts/ocr-verify-local.sh',
    'param-cmd': '${JJOB_SHARED_HOME}/jenkins/scripts/empty-cmd.sh',
    'keywords': ('ocr', 'percommit'),
    'timeout': 300,
    'sandbox': ('shared', 'shareOK'),
}

jobtype_ocr_verify_kernel_remote = {
    'name': 'ocr-verify-kernel-remote',
    'isLocal': True,
    'run-cmd': '${JJOB_ENVDIR}/jenkins/scripts/fsim-verify.sh',
    'param-cmd': '${JJOB_SHARED_HOME}/jenkins/scripts/empty-cmd.sh',
    'keywords': ('ocr', 'percommit'),
    'timeout': 300,
    'sandbox': ('shared', 'shareOK'),
}

# Specific jobs

# Fibonacci

job_ocr_build_kernel_fib_x86 = {
    'name': 'ocr-build-kernel-fib-x86',
    'depends': ('ocr-build-x86-pthread-x86',),
    'jobtype': 'ocr-build-kernel',
    'run-args': 'fib x86-pthread-x86',
    'sandbox': ('inherit0',),
    'env-vars': { 'APPS_LIBS_ROOT': '${APPS_ROOT}/libs/x86',
                  'WORKLOAD_SRC': '${JJOB_INITDIR}/ocr/ocr-apps/fib',
                  'WORKLOAD_BUILD_ROOT': '${JJOB_PRIVATE_HOME}/ocr/ocr-apps/fib/build',
                  'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/ocr/ocr-apps/fib/install'}
}

job_ocr_run_kernel_fib_x86 = {
    'name': 'ocr-run-kernel-fib-x86',
    'depends': ('ocr-build-kernel-fib-x86',),
    'jobtype': 'ocr-run-kernel-local',
    'run-args': 'fib x86-pthread-x86',
    'sandbox': ('inherit0',),
    'env-vars': { 'APPS_LIBS_ROOT': '${APPS_ROOT}/libs/x86',
                  'WORKLOAD_SRC': '${JJOB_INITDIR}/ocr/ocr-apps/fib',
                  'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/ocr/ocr-apps/fib/install' }
}

job_ocr_verify_kernel_fib_x86 = {
    'name': 'ocr-verify-kernel-fib-x86',
    'depends': ('ocr-run-kernel-fib-x86',),
    'jobtype': 'ocr-verify-kernel-local',
    'run-args': '-w -c 1',
    'sandbox': ('inherit0',),
    'env-vars': { 'WORKLOAD_EXEC': '${JJOB_SHARED_HOME}/ocr/ocr-apps/fib/install/x86-pthread-x86'}
}

job_ocr_build_kernel_fib_tgemul = {
    'name': 'ocr-build-kernel-fib-tgemul',
    'depends': ('ocr-build-x86-pthread-tg',),
    'jobtype': 'ocr-build-kernel',
    'run-args': 'fib x86-pthread-tg',
    'sandbox': ('inherit0',),
    'env-vars': { 'APPS_LIBS_ROOT': '${APPS_ROOT}/libs/x86',
                  'WORKLOAD_SRC': '${JJOB_INITDIR}/ocr/ocr-apps/fib',
                  'WORKLOAD_BUILD_ROOT': '${JJOB_PRIVATE_HOME}/ocr/ocr-apps/fib/build',
                  'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/ocr/ocr-apps/fib/install'}
}

job_ocr_run_kernel_fib_tgemul = {
    'name': 'ocr-run-kernel-fib-tgemul',
    'depends': ('ocr-build-kernel-fib-tgemul',),
    'jobtype': 'ocr-run-kernel-local',
    'run-args': 'fib x86-pthread-tg',
    'sandbox': ('inherit0',),
    'env-vars': { 'APPS_LIBS_ROOT': '${APPS_ROOT}/libs/x86',
                  'WORKLOAD_SRC': '${JJOB_INITDIR}/ocr/ocr-apps/fib',
                  'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/ocr/ocr-apps/fib/install'}
}

job_ocr_verify_kernel_fib_tgemul = {
    'name': 'ocr-verify-kernel-fib-tgemul',
    'depends': ('ocr-run-kernel-fib-tgemul',),
    'jobtype': 'ocr-verify-kernel-local',
    'run-args': '-w -c 1',
    'sandbox': ('inherit0',),
    'env-vars': { 'WORKLOAD_EXEC': '${JJOB_SHARED_HOME}/ocr/ocr-apps/fib/install/x86-pthread-tg'}
}

job_ocr_build_kernel_fib_tg = {
    'name': 'ocr-build-kernel-fib-tg',
    'depends': ('ocr-build-x86-builder-tg-ce', 'ocr-build-x86-builder-tg-xe',
                'ocr-build-tg-null-tg-ce', 'ocr-build-tg-null-tg-xe'),
    'jobtype': 'ocr-build-kernel',
    'run-args': 'fib tg',
    'sandbox': ('inherit0',),
    'env-vars': { 'APPS_LIBS_ROOT': '${APPS_ROOT}/libs/tg',
                  'WORKLOAD_SRC': '${JJOB_INITDIR}/ocr/ocr-apps/fib',
                  'WORKLOAD_BUILD_ROOT': '${JJOB_PRIVATE_HOME}/ocr/ocr-apps/fib/build',
                  'WORKLOAD_ARGS': '9',
                  'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/ocr/ocr-apps/fib/install'}
}

job_ocr_run_kernel_fib_tg = {
    'name': 'ocr-run-kernel-fib-tg',
    'depends': ('ocr-build-kernel-fib-tg',),
    'jobtype': 'ocr-run-kernel-remote',
    'run-args': 'fib tg',
    'param-args': '-c ${WORKLOAD_INSTALL_ROOT}/tg/config.cfg',
    'sandbox': ('inherit0',),
    'env-vars': { 'APPS_LIBS_ROOT': '${APPS_ROOT}/libs/tg',
                  'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/ocr/ocr-apps/fib/install'}
}

job_ocr_verify_kernel_fib_tg = {
    'name': 'ocr-verify-kernel-fib-tg',
    'depends': ('ocr-run-kernel-fib-tg',),
    'jobtype': 'ocr-verify-kernel-remote',
    'run-args': '-w -c 1',
    'sandbox': ('inherit0',),
    'env-vars': { 'WORKLOAD_EXEC': '${JJOB_SHARED_HOME}/ocr/ocr-apps/fib/install/tg'}
}

# FFT

job_ocr_build_kernel_fft_x86 = {
    'name': 'ocr-build-kernel-fft-x86',
    'depends': ('ocr-build-x86-pthread-x86',),
    'jobtype': 'ocr-build-kernel',
    'run-args': 'fft x86-pthread-x86',
    'sandbox': ('inherit0',),
    'env-vars': { 'APPS_LIBS_ROOT': '${APPS_ROOT}/libs/x86',
                  'WORKLOAD_SRC': '${JJOB_INITDIR}/ocr/ocr-apps/fft',
                  'WORKLOAD_BUILD_ROOT': '${JJOB_PRIVATE_HOME}/ocr/ocr-apps/fft/build',
                  'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/ocr/ocr-apps/fft/install'}
}

job_ocr_run_kernel_fft_x86 = {
    'name': 'ocr-run-kernel-fft-x86',
    'depends': ('ocr-build-kernel-fft-x86',),
    'jobtype': 'ocr-run-kernel-local',
    'run-args': 'fft x86-pthread-x86',
    'sandbox': ('inherit0',),
    'env-vars': { 'APPS_LIBS_ROOT': '${APPS_ROOT}/libs/x86',
                  'WORKLOAD_SRC': '${JJOB_INITDIR}/ocr/ocr-apps/fft',
                  'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/ocr/ocr-apps/fft/install' }
}

job_ocr_verify_kernel_fft_x86 = {
    'name': 'ocr-verify-kernel-fft-x86',
    'depends': ('ocr-run-kernel-fft-x86',),
    'jobtype': 'ocr-verify-kernel-local',
    'run-args': '-w -c 1',
    'sandbox': ('inherit0',),
    'env-vars': { 'WORKLOAD_EXEC': '${JJOB_SHARED_HOME}/ocr/ocr-apps/fft/install/x86-pthread-x86'}
}

job_ocr_build_kernel_fft_tgemul = {
    'name': 'ocr-build-kernel-fft-tgemul',
    'depends': ('ocr-build-x86-pthread-tg',),
    'jobtype': 'ocr-build-kernel',
    'run-args': 'fft x86-pthread-tg',
    'sandbox': ('inherit0',),
    'env-vars': { 'APPS_LIBS_ROOT': '${APPS_ROOT}/libs/x86',
                  'WORKLOAD_SRC': '${JJOB_INITDIR}/ocr/ocr-apps/fft',
                  'WORKLOAD_BUILD_ROOT': '${JJOB_PRIVATE_HOME}/ocr/ocr-apps/fft/build',
                  'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/ocr/ocr-apps/fft/install'}
}

job_ocr_run_kernel_fft_tgemul = {
    'name': 'ocr-run-kernel-fft-tgemul',
    'depends': ('ocr-build-kernel-fft-tgemul',),
    'jobtype': 'ocr-run-kernel-local',
    'run-args': 'fft x86-pthread-tg',
    'sandbox': ('inherit0',),
    'env-vars': { 'APPS_LIBS_ROOT': '${APPS_ROOT}/libs/x86',
                  'WORKLOAD_SRC': '${JJOB_INITDIR}/ocr/ocr-apps/fft',
                  'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/ocr/ocr-apps/fft/install'}
}

job_ocr_verify_kernel_fft_tgemul = {
    'name': 'ocr-verify-kernel-fft-tgemul',
    'depends': ('ocr-run-kernel-fft-tgemul',),
    'jobtype': 'ocr-verify-kernel-local',
    'run-args': '-w -c 1',
    'sandbox': ('inherit0',),
    'env-vars': { 'WORKLOAD_EXEC': '${JJOB_SHARED_HOME}/ocr/ocr-apps/fft/install/x86-pthread-tg'}
}

job_ocr_build_kernel_fft_tg = {
    'name': 'ocr-build-kernel-fft-tg',
    'depends': ('ocr-build-x86-builder-tg-ce', 'ocr-build-x86-builder-tg-xe',
                'ocr-build-tg-null-tg-ce', 'ocr-build-tg-null-tg-xe'),
    'jobtype': 'ocr-build-kernel',
    'run-args': 'fft tg',
    'sandbox': ('inherit0',),
    'env-vars': { 'APPS_LIBS_ROOT': '${APPS_ROOT}/libs/tg',
                  'WORKLOAD_SRC': '${JJOB_INITDIR}/ocr/ocr-apps/fft',
                  'WORKLOAD_BUILD_ROOT': '${JJOB_PRIVATE_HOME}/ocr/ocr-apps/fft/build',
                  'WORKLOAD_ARGS': '9',
                  'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/ocr/ocr-apps/fft/install'}
}

job_ocr_run_kernel_fft_tg = {
    'name': 'ocr-run-kernel-fft-tg',
    'depends': ('ocr-build-kernel-fft-tg',),
    'jobtype': 'ocr-run-kernel-remote',
    'run-args': 'fft tg',
    'param-args': '-c ${WORKLOAD_INSTALL_ROOT}/tg/config.cfg',
    'sandbox': ('inherit0',),
    'env-vars': { 'APPS_LIBS_ROOT': '${APPS_ROOT}/libs/tg',
                  'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/ocr/ocr-apps/fft/install'}
}

job_ocr_verify_kernel_fft_tg = {
    'name': 'ocr-verify-kernel-fft-tg',
    'depends': ('ocr-run-kernel-fft-tg',),
    'jobtype': 'ocr-verify-kernel-remote',
    'run-args': '-w -c 1',
    'sandbox': ('inherit0',),
    'env-vars': { 'WORKLOAD_EXEC': '${JJOB_SHARED_HOME}/ocr/ocr-apps/fft/install/tg'}
}