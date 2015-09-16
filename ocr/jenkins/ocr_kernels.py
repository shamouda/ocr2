#!/usr/bin/env python

# This relies on stuff (OCR) being built in the shared
# home but does not need any separate checkout as it
# runs completely out of the source directory
jobtype_ocr_build_kernel = {
    'name': 'ocr-build-kernel',
    'isLocal': True,
    'run-cmd': '${JJOB_SHARED_HOME}/xstack/ocr/jenkins/scripts/kernel-build.sh',
    'param-cmd': '${JJOB_SHARED_HOME}/xstack/jenkins/scripts/empty-cmd.sh',
    'keywords': ('ocr', 'percommit'),
    'timeout': 300,
    'sandbox': ('local', 'shared', 'shareOK'),
    'req-repos': ('xstack',),
    'env-vars': { 'APPS_ROOT': '${JJOB_INITDIR_XSTACK}/apps',
                  'OCR_ROOT': '${JJOB_PRIVATE_HOME}/xstack/ocr',
                  'OCR_INSTALL_ROOT': '${JJOB_SHARED_HOME}/xstack/ocr/install',
                  'OCR_BUILD_ROOT': '${JJOB_PRIVATE_HOME}/xstack/ocr/build'}
}

jobtype_ocr_build_kernel_tg = {
    'name': 'ocr-build-kernel-tg',
    'isLocal': True,
    'run-cmd': '${JJOB_SHARED_HOME}/xstack/ocr/jenkins/scripts/kernel-build.sh',
    'param-cmd': '${JJOB_SHARED_HOME}/xstack/jenkins/scripts/empty-cmd.sh',
    'keywords': ('ocr', 'percommit'),
    'timeout': 300,
    'sandbox': ('local', 'shared', 'shareOK'),
    'req-repos': ('xstack', 'intel'),
    'env-vars': { 'TG_INSTALL': '${JJOB_ENVDIR}',
                  'TG_ROOT': '${JJOB_INITDIR_INTEL}/ss',
                  'APPS_ROOT': '${JJOB_INITDIR_XSTACK}/apps',
                  'OCR_ROOT': '${JJOB_PRIVATE_HOME}/xstack/ocr',
                  'OCR_INSTALL_ROOT': '${JJOB_SHARED_HOME}/xstack/ocr/install',
                  'OCR_BUILD_ROOT': '${JJOB_PRIVATE_HOME}/xstack/ocr/build'}
}

jobtype_ocr_run_kernel_local = {
    'name': 'ocr-run-kernel-local',
    'isLocal': True,
    'run-cmd': '${JJOB_SHARED_HOME}/xstack/ocr/jenkins/scripts/kernel-run-local.sh',
    'param-cmd': '${JJOB_SHARED_HOME}/xstack/jenkins/scripts/empty-cmd.sh',
    'keywords': ('ocr', 'percommit'),
    'timeout': 300,
    'sandbox': ('shared', 'shareOK'),
    'req-repos': ('xstack',),
    'env-vars': { 'APPS_ROOT': '${JJOB_SHARED_HOME}/xstack/apps', # Use this for the makefiles
                  'OCR_ROOT': '${JJOB_SHARED_HOME}/xstack/ocr',
                  'OCR_BUILD_ROOT': '${JJOB_PARENT_PRIVATE_HOME_0}/xstack/ocr/build',
                  'OCR_INSTALL_ROOT': '${JJOB_SHARED_HOME}/xstack/ocr/install'}
}

jobtype_ocr_run_kernel_remote_tg = {
    'name': 'ocr-run-kernel-remote-tg',
    'isLocal': False,
    'run-cmd': '${JJOB_SHARED_HOME}/xstack/ocr/jenkins/scripts/kernel-run-remote.sh',
    'param-cmd': '${JJOB_SHARED_HOME}/intel/ss/jenkins/scripts/fsim-param-cmd.sh',
    'epilogue-cmd': '${JJOB_ENVDIR}/bin/fsim-scripts/fsim-epilogue.sh',
    'keywords': ('ocr', 'percommit'),
    'timeout': 300,
    'sandbox': ('shared', 'shareOK'),
    'req-repos': ('xstack', 'intel'),
    'env-vars': { 'TG_INSTALL': '${JJOB_ENVDIR}',
                  'APPS_ROOT': '${JJOB_SHARED_HOME}/xstack/apps', # Use this for the makefiles
                  'OCR_INSTALL_ROOT': '${JJOB_SHARED_HOME}/xstack/ocr/install'}
}

jobtype_ocr_verify_kernel_local = {
    'name': 'ocr-verify-kernel-local',
    'isLocal': True,
    'run-cmd': '${JJOB_SHARED_HOME}/xstack/ocr/jenkins/scripts/ocr-verify-local.sh',
    'param-cmd': '${JJOB_SHARED_HOME}/xstack/jenkins/scripts/empty-cmd.sh',
    'keywords': ('ocr', 'percommit'),
    'timeout': 300,
    'sandbox': ('shared', 'shareOK'),
    'req-repos': ('xstack',)
}

jobtype_ocr_verify_kernel_remote = {
    'name': 'ocr-verify-kernel-remote',
    'isLocal': True,
    'run-cmd': '${JJOB_SHARED_HOME}/xstack/ocr/jenkins/scripts/fsim-verify.sh',
    'param-cmd': '${JJOB_SHARED_HOME}/xstack/jenkins/scripts/empty-cmd.sh',
    'keywords': ('ocr', 'percommit'),
    'timeout': 300,
    'sandbox': ('shared', 'shareOK'),
    'req-repos': ('xstack',)
}

jobtype_ocr_verify_by_diff = {
    'name': 'ocr-verify-diff',
    'isLocal': True,
    'run-cmd': '${JJOB_SHARED_HOME}/xstack/ocr/jenkins/scripts/verify-diff.sh',
    'param-cmd': '${JJOB_SHARED_HOME}/xstack/jenkins/scripts/empty-cmd.sh',
    'keywords': ('ocr', 'percommit'),
    'timeout': 300,
    'sandbox': ('shared', 'shareOK'),
    'req-repos': ('xstack',),
    'env-vars': { 'APPS_ROOT': '${JJOB_INITDIR_XSTACK}/apps' }
}


# # Specific jobs

# # Fibonacci

job_ocr_build_kernel_fib_x86 = {
    'name': 'ocr-build-kernel-fib-x86',
    'depends': ('ocr-build-x86',),
    'jobtype': 'ocr-build-kernel',
    'run-args': 'fib x86',
    'sandbox': ('inherit0',),
    'env-vars': { 'APPS_LIBS_ROOT': '${JJOB_SHARED_HOME}/xstack/apps/libs/x86',
                  'WORKLOAD_SRC': '${JJOB_INITDIR_XSTACK}/ocr/examples/fib',
                  'WORKLOAD_BUILD_ROOT': '${JJOB_PRIVATE_HOME}/xstack/ocr/examples/fib/build',
                  'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/xstack/ocr/examples/fib/install'}
}

job_ocr_run_kernel_fib_x86 = {
    'name': 'ocr-run-kernel-fib-x86',
    'depends': ('ocr-build-kernel-fib-x86',),
    'jobtype': 'ocr-run-kernel-local',
    'run-args': 'fib x86',
    'sandbox': ('inherit0',),
    'env-vars': { 'APPS_LIBS_ROOT': '${JJOB_SHARED_HOME}/xstack/apps/libs/x86',
                  'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/xstack/ocr/examples/fib/install' }
}

job_ocr_verify_kernel_fib_x86 = {
    'name': 'ocr-verify-kernel-fib-x86',
    'depends': ('ocr-run-kernel-fib-x86',),
    'jobtype': 'ocr-verify-kernel-local',
    'run-args': '-w -c 1',
    'sandbox': ('inherit0',),
    'env-vars': { 'WORKLOAD_EXEC': '${JJOB_SHARED_HOME}/xstack/ocr/examples/fib/install/x86'}
}

job_ocr_build_kernel_fib_mpi = {
    'name': 'ocr-build-kernel-fib-mpi',
    'depends': ('ocr-build-x86-mpi',),
    'jobtype': 'ocr-build-kernel',
    'run-args': 'fib x86-mpi',
    'sandbox': ('inherit0',),
    'env-vars': { 'APPS_LIBS_ROOT': '${JJOB_SHARED_HOME}/xstack/apps/libs/x86',
                  'WORKLOAD_SRC': '${JJOB_INITDIR_XSTACK}/ocr/examples/fib',
                  'WORKLOAD_BUILD_ROOT': '${JJOB_PRIVATE_HOME}/xstack/ocr/examples/fib/build',
                  'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/xstack/ocr/examples/fib/install'}
}

job_ocr_run_kernel_fib_mpi = {
    'name': 'ocr-run-kernel-fib-mpi',
    'depends': ('ocr-build-kernel-fib-mpi',),
    'jobtype': 'ocr-run-kernel-local',
    'run-args': 'fib x86-mpi',
    'sandbox': ('inherit0',),
    'env-vars': { 'PATH': '/opt/intel/tools/impi/5.1.1.109/intel64/bin:${PATH}',
                  'APPS_LIBS_ROOT': '${JJOB_SHARED_HOME}/xstack/apps/libs/x86',
                  'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/xstack/ocr/examples/fib/install' }
}

job_ocr_verify_kernel_fib_mpi = {
    'name': 'ocr-verify-kernel-fib-mpi',
    'depends': ('ocr-run-kernel-fib-mpi',),
    'jobtype': 'ocr-verify-kernel-local',
    'run-args': '-w -c 1',
    'sandbox': ('inherit0',),
    'env-vars': { 'WORKLOAD_EXEC': '${JJOB_SHARED_HOME}/xstack/ocr/examples/fib/install/x86-mpi'}
}

job_ocr_build_kernel_fib_gasnet = {
    'name': 'ocr-build-kernel-fib-gasnet',
    'depends': ('ocr-build-x86-gasnet',),
    'jobtype': 'ocr-build-kernel',
    'run-args': 'fib x86-gasnet',
    'sandbox': ('inherit0',),
    'env-vars': { 'APPS_LIBS_ROOT': '${JJOB_SHARED_HOME}/xstack/apps/libs/x86',
                  'WORKLOAD_SRC': '${JJOB_INITDIR_XSTACK}/ocr/examples/fib',
                  'WORKLOAD_BUILD_ROOT': '${JJOB_PRIVATE_HOME}/xstack/ocr/examples/fib/build',
                  'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/xstack/ocr/examples/fib/install',
                  'MPI_ROOT': '/opt/intel/tools/impi/5.1.1.109/intel64',
                  'GASNET_ROOT': '/opt/rice/GASNet/1.24.0-impi',
                  'PATH': '${GASNET_ROOT}/bin:${MPI_ROOT}/bin:${PATH}',
                  'GASNET_CONDUIT': 'ibv',
                  'GASNET_TYPE': 'par',
                  'GASNET_EXTRA_LIBS': '-L/usr/lib64 -lrt -libverbs',
                  'CC': 'mpicc', # gasnet built with mpi
                  }
}

job_ocr_run_kernel_fib_gasnet = {
    'name': 'ocr-run-kernel-fib-gasnet',
    'depends': ('ocr-build-kernel-fib-gasnet',),
    'jobtype': 'ocr-run-kernel-local',
    'run-args': 'fib x86-gasnet',
    'sandbox': ('inherit0',),
    'env-vars': { 'PATH': '/opt/intel/tools/impi/5.1.1.109/intel64/bin:${PATH}',
                  'APPS_LIBS_ROOT': '${JJOB_SHARED_HOME}/xstack/apps/libs/x86',
                  'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/xstack/ocr/examples/fib/install',
                  'GASNET_ROOT': '/opt/rice/GASNet/1.24.0-impi',
                  'PATH': '${GASNET_ROOT}/bin:${MPI_ROOT}/bin:${PATH}',
                  'GASNET_CONDUIT': 'ibv'}
}

job_ocr_verify_kernel_fib_gasnet = {
    'name': 'ocr-verify-kernel-fib-gasnet',
    'depends': ('ocr-run-kernel-fib-gasnet',),
    'jobtype': 'ocr-verify-kernel-local',
    'run-args': '-w -c 1',
    'sandbox': ('inherit0',),
    'env-vars': { 'WORKLOAD_EXEC': '${JJOB_SHARED_HOME}/xstack/ocr/examples/fib/install/x86-gasnet'}
}

# job_ocr_build_kernel_fib_tgemul = {
#     'name': 'ocr-build-kernel-fib-tgemul',
#     'depends': ('ocr-build-tg-x86',),
#     'jobtype': 'ocr-build-kernel',
#     'run-args': 'fib tg-x86',
#     'sandbox': ('inherit0',),
#     'env-vars': { 'APPS_LIBS_ROOT': '${JJOB_SHARED_HOME}/xstack/apps/libs/x86',
#                   'WORKLOAD_SRC': '${JJOB_INITDIR_XSTACK}/ocr/examples/fib',
#                   'WORKLOAD_BUILD_ROOT': '${JJOB_PRIVATE_HOME}/xstack/ocr/examples/fib/build',
#                   'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/xstack/ocr/examples/fib/install'}
# }

# job_ocr_run_kernel_fib_tgemul = {
#     'name': 'ocr-run-kernel-fib-tgemul',
#     'depends': ('ocr-build-kernel-fib-tgemul',),
#     'jobtype': 'ocr-run-kernel-local',
#     'run-args': 'fib tg-x86',
#     'sandbox': ('inherit0',),
#     'env-vars': { 'APPS_LIBS_ROOT': '${JJOB_SHARED_HOME}/xstack/apps/libs/x86',
#                   'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/xstack/ocr/examples/fib/install'}
# }

# job_ocr_verify_kernel_fib_tgemul = {
#     'name': 'ocr-verify-kernel-fib-tgemul',
#     'depends': ('ocr-run-kernel-fib-tgemul',),
#     'jobtype': 'ocr-verify-kernel-local',
#     'run-args': '-w -c 1',
#     'sandbox': ('inherit0',),
#     'env-vars': { 'WORKLOAD_EXEC': '${JJOB_SHARED_HOME}/xstack/ocr/examples/fib/install/tg-x86'}
# }

job_ocr_build_kernel_fib_tg = {
    'name': 'ocr-build-kernel-fib-tg',
    'depends': ('ocr-build-builder-ce', 'ocr-build-builder-xe',
                'ocr-build-tg-ce', 'ocr-build-tg-xe'),
    'jobtype': 'ocr-build-kernel-tg',
    'run-args': 'fib tg',
    'sandbox': ('inherit0',),
    'env-vars': { 'APPS_LIBS_ROOT': '${JJOB_SHARED_HOME}/xstack/apps/libs/tg',
                  'WORKLOAD_SRC': '${JJOB_INITDIR_XSTACK}/ocr/examples/fib',
                  'WORKLOAD_BUILD_ROOT': '${JJOB_PRIVATE_HOME}/xstack/ocr/examples/fib/build',
                  'WORKLOAD_ARGS': '9',
                  'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/xstack/ocr/examples/fib/install'}
}

job_ocr_run_kernel_fib_tg = {
    'name': 'ocr-run-kernel-fib-tg',
    'depends': ('ocr-build-kernel-fib-tg',),
    'jobtype': 'ocr-run-kernel-remote-tg',
    'run-args': 'fib tg',
    'param-args': '-c ${WORKLOAD_INSTALL_ROOT}/tg/config.cfg',
    'sandbox': ('inherit0',),
    'env-vars': { 'APPS_LIBS_ROOT': '${JJOB_SHARED_HOME}/xstack/apps/libs/tg',
                  'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/xstack/ocr/examples/fib/install'}
}

job_ocr_verify_kernel_fib_tg = {
    'name': 'ocr-verify-kernel-fib-tg',
    'depends': ('ocr-run-kernel-fib-tg',),
    'jobtype': 'ocr-verify-kernel-remote',
    'run-args': '-w -c 1',
    'sandbox': ('inherit0',),
    'env-vars': { 'WORKLOAD_EXEC': '${JJOB_SHARED_HOME}/xstack/ocr/examples/fib/install/tg'}
}

# # FFT

job_ocr_build_kernel_fft_x86 = {
    'name': 'ocr-build-kernel-fft-x86',
    'depends': ('ocr-build-x86',),
    'jobtype': 'ocr-build-kernel',
    'run-args': 'fft x86',
    'sandbox': ('inherit0',),
    'env-vars': { 'APPS_LIBS_ROOT': '${JJOB_SHARED_HOME}/xstack/apps/libs/x86',
                  'WORKLOAD_SRC': '${JJOB_INITDIR_XSTACK}/ocr/examples/fft',
                  'WORKLOAD_BUILD_ROOT': '${JJOB_PRIVATE_HOME}/xstack/ocr/examples/fft/build',
                  'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/xstack/ocr/examples/fft/install'}
}

job_ocr_run_kernel_fft_x86 = {
    'name': 'ocr-run-kernel-fft-x86',
    'depends': ('ocr-build-kernel-fft-x86',),
    'jobtype': 'ocr-run-kernel-local',
    'run-args': 'fft x86',
    'sandbox': ('inherit0',),
    'env-vars': { 'APPS_LIBS_ROOT': '${JJOB_SHARED_HOME}/xstack/apps/libs/x86',
                  'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/xstack/ocr/examples/fft/install' }
}

job_ocr_verify_kernel_fft_x86 = {
    'name': 'ocr-verify-kernel-fft-x86',
    'depends': ('ocr-run-kernel-fft-x86',),
    'jobtype': 'ocr-verify-kernel-local',
    'run-args': '-w -c 1',
    'sandbox': ('inherit0',),
    'env-vars': { 'WORKLOAD_EXEC': '${JJOB_SHARED_HOME}/xstack/ocr/examples/fft/install/x86'}
}

job_ocr_build_kernel_fft_mpi = {
    'name': 'ocr-build-kernel-fft-mpi',
    'depends': ('ocr-build-x86-mpi',),
    'jobtype': 'ocr-build-kernel',
    'run-args': 'fft x86-mpi',
    'sandbox': ('inherit0',),
    'env-vars': { 'APPS_LIBS_ROOT': '${JJOB_SHARED_HOME}/xstack/apps/libs/x86',
                  'WORKLOAD_SRC': '${JJOB_INITDIR_XSTACK}/ocr/examples/fft',
                  'WORKLOAD_BUILD_ROOT': '${JJOB_PRIVATE_HOME}/xstack/ocr/examples/fft/build',
                  'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/xstack/ocr/examples/fft/install'}
}

job_ocr_run_kernel_fft_mpi = {
    'name': 'ocr-run-kernel-fft-mpi',
    'depends': ('ocr-build-kernel-fft-mpi',),
    'jobtype': 'ocr-run-kernel-local',
    'run-args': 'fft x86-mpi',
    'sandbox': ('inherit0',),
    'env-vars': { 'PATH': '/opt/intel/tools/impi/5.1.1.109/intel64/bin:${PATH}',
                  'APPS_LIBS_ROOT': '${JJOB_SHARED_HOME}/xstack/apps/libs/x86',
                  'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/xstack/ocr/examples/fft/install' }
}

job_ocr_verify_kernel_fft_mpi = {
    'name': 'ocr-verify-kernel-fft-mpi',
    'depends': ('ocr-run-kernel-fft-mpi',),
    'jobtype': 'ocr-verify-kernel-local',
    'run-args': '-w -c 1',
    'sandbox': ('inherit0',),
    'env-vars': { 'WORKLOAD_EXEC': '${JJOB_SHARED_HOME}/xstack/ocr/examples/fft/install/x86-mpi'}
}

job_ocr_build_kernel_fft_gasnet = {
    'name': 'ocr-build-kernel-fft-gasnet',
    'depends': ('ocr-build-x86-gasnet',),
    'jobtype': 'ocr-build-kernel',
    'run-args': 'fft x86-gasnet',
    'sandbox': ('inherit0',),
    'env-vars': { 'APPS_LIBS_ROOT': '${JJOB_SHARED_HOME}/xstack/apps/libs/x86',
                  'WORKLOAD_SRC': '${JJOB_INITDIR_XSTACK}/ocr/examples/fft',
                  'WORKLOAD_BUILD_ROOT': '${JJOB_PRIVATE_HOME}/xstack/ocr/examples/fft/build',
                  'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/xstack/ocr/examples/fft/install',
                  'MPI_ROOT': '/opt/intel/tools/impi/5.1.1.109/intel64',
                  'GASNET_ROOT': '/opt/rice/GASNet/1.24.0-impi',
                  'PATH': '${GASNET_ROOT}/bin:${MPI_ROOT}/bin:${PATH}',
                  'GASNET_CONDUIT': 'ibv',
                  'GASNET_TYPE': 'par',
                  'GASNET_EXTRA_LIBS': '-L/usr/lib64 -lrt -libverbs',
                  'CC': 'mpicc', # gasnet built with mpi
                  }
}

job_ocr_run_kernel_fft_gasnet = {
    'name': 'ocr-run-kernel-fft-gasnet',
    'depends': ('ocr-build-kernel-fft-gasnet',),
    'jobtype': 'ocr-run-kernel-local',
    'run-args': 'fft x86-gasnet',
    'sandbox': ('inherit0',),
    'env-vars': { 'PATH': '/opt/intel/tools/impi/5.1.1.109/intel64/bin:${PATH}',
                  'APPS_LIBS_ROOT': '${JJOB_SHARED_HOME}/xstack/apps/libs/x86',
                  'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/xstack/ocr/examples/fft/install',
                  'GASNET_ROOT': '/opt/rice/GASNet/1.24.0-impi',
                  'PATH': '${GASNET_ROOT}/bin:${MPI_ROOT}/bin:${PATH}',
                  'GASNET_CONDUIT': 'ibv'}
}

job_ocr_verify_kernel_fft_gasnet = {
    'name': 'ocr-verify-kernel-fft-gasnet',
    'depends': ('ocr-run-kernel-fft-gasnet',),
    'jobtype': 'ocr-verify-kernel-local',
    'run-args': '-w -c 1',
    'sandbox': ('inherit0',),
    'env-vars': { 'WORKLOAD_EXEC': '${JJOB_SHARED_HOME}/xstack/ocr/examples/fft/install/x86-gasnet'}
}

# job_ocr_build_kernel_fft_tgemul = {
#     'name': 'ocr-build-kernel-fft-tgemul',
#     'depends': ('ocr-build-tg-x86',),
#     'jobtype': 'ocr-build-kernel',
#     'run-args': 'fft tg-x86',
#     'sandbox': ('inherit0',),
#     'env-vars': { 'APPS_LIBS_ROOT': '${JJOB_SHARED_HOME}/xstack/apps/libs/x86',
#                   'WORKLOAD_SRC': '${JJOB_INITDIR_XSTACK}/ocr/examples/fft',
#                   'WORKLOAD_BUILD_ROOT': '${JJOB_PRIVATE_HOME}/xstack/ocr/examples/fft/build',
#                   'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/xstack/ocr/examples/fft/install'}
# }

# job_ocr_run_kernel_fft_tgemul = {
#     'name': 'ocr-run-kernel-fft-tgemul',
#     'depends': ('ocr-build-kernel-fft-tgemul',),
#     'jobtype': 'ocr-run-kernel-local',
#     'run-args': 'fft tg-x86',
#     'sandbox': ('inherit0',),
#     'env-vars': { 'APPS_LIBS_ROOT': '${JJOB_SHARED_HOME}/xstack/apps/libs/x86',
#                   'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/xstack/ocr/examples/fft/install'}
# }

# job_ocr_verify_kernel_fft_tgemul = {
#     'name': 'ocr-verify-kernel-fft-tgemul',
#     'depends': ('ocr-run-kernel-fft-tgemul',),
#     'jobtype': 'ocr-verify-kernel-local',
#     'run-args': '-w -c 1',
#     'sandbox': ('inherit0',),
#     'env-vars': { 'WORKLOAD_EXEC': '${JJOB_SHARED_HOME}/xstack/ocr/examples/fft/install/tg-x86'}
# }

job_ocr_build_kernel_fft_tg = {
    'name': 'ocr-build-kernel-fft-tg',
    'depends': ('ocr-build-builder-ce', 'ocr-build-builder-xe',
                'ocr-build-tg-ce', 'ocr-build-tg-xe'),
    'jobtype': 'ocr-build-kernel-tg',
    'run-args': 'fft tg',
    'sandbox': ('inherit0',),
    'env-vars': { 'APPS_LIBS_ROOT': '${JJOB_SHARED_HOME}/xstack/apps/libs/tg',
                  'WORKLOAD_SRC': '${JJOB_INITDIR_XSTACK}/ocr/examples/fft',
                  'WORKLOAD_BUILD_ROOT': '${JJOB_PRIVATE_HOME}/xstack/ocr/examples/fft/build',
                  'WORKLOAD_ARGS': '9',
                  'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/xstack/ocr/examples/fft/install'}
}

job_ocr_run_kernel_fft_tg = {
    'name': 'ocr-run-kernel-fft-tg',
    'depends': ('ocr-build-kernel-fft-tg',),
    'jobtype': 'ocr-run-kernel-remote-tg',
    'run-args': 'fft tg',
    'param-args': '-c ${WORKLOAD_INSTALL_ROOT}/tg/config.cfg',
    'sandbox': ('inherit0',),
    'env-vars': { 'APPS_LIBS_ROOT': '${JJOB_SHARED_HOME}/xstack/apps/libs/tg',
                  'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/xstack/ocr/examples/fft/install'}
}

job_ocr_verify_kernel_fft_tg = {
    'name': 'ocr-verify-kernel-fft-tg',
    'depends': ('ocr-run-kernel-fft-tg',),
    'jobtype': 'ocr-verify-kernel-remote',
    'run-args': '-w -c 1',
    'sandbox': ('inherit0',),
    'env-vars': { 'WORKLOAD_EXEC': '${JJOB_SHARED_HOME}/xstack/ocr/examples/fft/install/tg'}
}

# Smith-Waterman

job_ocr_build_kernel_sw_x86 = {
    'name': 'ocr-build-kernel-sw-x86',
    'depends': ('ocr-build-x86',),
    'jobtype': 'ocr-build-kernel',
    'run-args': 'smith-waterman x86',
    'sandbox': ('inherit0',),
    'env-vars': { 'APPS_LIBS_ROOT': '${JJOB_SHARED_HOME}/xstack/apps/libs/x86',
                  'WORKLOAD_SRC': '${JJOB_INITDIR_XSTACK}/ocr/examples/smith-waterman',
                  'WORKLOAD_BUILD_ROOT': '${JJOB_PRIVATE_HOME}/xstack/ocr/examples/smith-waterman/build',
                  'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/xstack/ocr/examples/smith-waterman/install'}
}

job_ocr_run_kernel_sw_x86 = {
    'name': 'ocr-run-kernel-sw-x86',
    'depends': ('ocr-build-kernel-sw-x86',),
    'jobtype': 'ocr-run-kernel-local',
    'run-args': 'smith-waterman x86',
    'sandbox': ('inherit0',),
    'env-vars': { 'APPS_LIBS_ROOT': '${JJOB_SHARED_HOME}/xstack/apps/libs/x86',
                  'WORKLOAD_ARGS': '10 10 ${JJOB_SHARED_HOME}/xstack/apps/smithwaterman/datasets/string1-medium.txt  ${JJOB_SHARED_HOME}/xstack/apps/smithwaterman/datasets/string2-medium.txt ${JJOB_SHARED_HOME}/xstack/apps/smithwaterman/datasets/score-medium.txt',
                  'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/xstack/ocr/examples/smith-waterman/install' }
}

job_ocr_verify_kernel_sw_x86 = {
    'name': 'ocr-verify-kernel-sw-x86',
    'depends': ('ocr-run-kernel-sw-x86',),
    'jobtype': 'ocr-verify-kernel-local',
    'run-args': '-w -c 1',
    'sandbox': ('inherit0',),
    'env-vars': { 'WORKLOAD_EXEC': '${JJOB_SHARED_HOME}/xstack/ocr/examples/smith-waterman/install/x86'}
}

job_ocr_build_kernel_sw_mpi = {
    'name': 'ocr-build-kernel-sw-mpi',
    'depends': ('ocr-build-x86-mpi',),
    'jobtype': 'ocr-build-kernel',
    'run-args': 'smith-waterman x86-mpi',
    'sandbox': ('inherit0',),
    'env-vars': { 'APPS_LIBS_ROOT': '${JJOB_SHARED_HOME}/xstack/apps/libs/x86',
                  'WORKLOAD_SRC': '${JJOB_INITDIR_XSTACK}/ocr/examples/smith-waterman',
                  'WORKLOAD_BUILD_ROOT': '${JJOB_PRIVATE_HOME}/xstack/ocr/examples/smith-waterman/build',
                  'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/xstack/ocr/examples/smith-waterman/install'}
}

job_ocr_run_kernel_sw_mpi = {
    'name': 'ocr-run-kernel-sw-mpi',
    'depends': ('ocr-build-kernel-sw-mpi',),
    'jobtype': 'ocr-run-kernel-local',
    'run-args': 'smith-waterman x86-mpi',
    'sandbox': ('inherit0',),
    'env-vars': { 'PATH': '/opt/intel/tools/impi/5.1.1.109/intel64/bin:${PATH}',
                  'APPS_LIBS_ROOT': '${JJOB_SHARED_HOME}/xstack/apps/libs/x86',
                  'WORKLOAD_ARGS': '100 100 ${JJOB_SHARED_HOME}/xstack/apps/smithwaterman/datasets/string1-medium-large.txt  ${JJOB_SHARED_HOME}/xstack/apps/smithwaterman/datasets/string2-medium-large.txt ${JJOB_SHARED_HOME}/xstack/apps/smithwaterman/datasets/score-medium-large.txt',
                  'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/xstack/ocr/examples/smith-waterman/install' }
}

job_ocr_verify_kernel_sw_mpi = {
    'name': 'ocr-verify-kernel-sw-mpi',
    'depends': ('ocr-run-kernel-sw-mpi',),
    'jobtype': 'ocr-verify-kernel-local',
    'run-args': '-w -c 1',
    'sandbox': ('inherit0',),
    'env-vars': { 'WORKLOAD_EXEC': '${JJOB_SHARED_HOME}/xstack/ocr/examples/smith-waterman/install/x86-mpi'}
}


job_ocr_build_kernel_sw_gasnet = {
    'name': 'ocr-build-kernel-sw-gasnet',
    'depends': ('ocr-build-x86-gasnet',),
    'jobtype': 'ocr-build-kernel',
    'run-args': 'smith-waterman x86-gasnet',
    'sandbox': ('inherit0',),
    'env-vars': { 'APPS_LIBS_ROOT': '${JJOB_SHARED_HOME}/xstack/apps/libs/x86',
                  'WORKLOAD_SRC': '${JJOB_INITDIR_XSTACK}/ocr/examples/smith-waterman',
                  'WORKLOAD_BUILD_ROOT': '${JJOB_PRIVATE_HOME}/xstack/ocr/examples/smith-waterman/build',
                  'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/xstack/ocr/examples/smith-waterman/install',
                  'MPI_ROOT': '/opt/intel/tools/impi/5.1.1.109/intel64',
                  'GASNET_ROOT': '/opt/rice/GASNet/1.24.0-impi',
                  'PATH': '${GASNET_ROOT}/bin:${MPI_ROOT}/bin:${PATH}',
                  'GASNET_CONDUIT': 'ibv',
                  'GASNET_TYPE': 'par',
                  'GASNET_EXTRA_LIBS': '-L/usr/lib64 -lrt -libverbs',
                  'CC': 'mpicc', # gasnet built with mpi
                  }
}

job_ocr_run_kernel_sw_gasnet = {
    'name': 'ocr-run-kernel-sw-gasnet',
    'depends': ('ocr-build-kernel-sw-gasnet',),
    'jobtype': 'ocr-run-kernel-local',
    'run-args': 'smith-waterman x86-gasnet',
    'sandbox': ('inherit0',),
    'env-vars': { 'PATH': '/opt/intel/tools/impi/5.1.1.109/intel64/bin:${PATH}',
                  'APPS_LIBS_ROOT': '${JJOB_SHARED_HOME}/xstack/apps/libs/x86',
                  'WORKLOAD_ARGS': '100 100 ${JJOB_SHARED_HOME}/xstack/apps/smithwaterman/datasets/string1-medium-large.txt  ${JJOB_SHARED_HOME}/xstack/apps/smithwaterman/datasets/string2-medium-large.txt ${JJOB_SHARED_HOME}/xstack/apps/smithwaterman/datasets/score-medium-large.txt',
                  'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/xstack/ocr/examples/smith-waterman/install',
                  'GASNET_ROOT': '/opt/rice/GASNet/1.24.0-impi',
                  'PATH': '${GASNET_ROOT}/bin:${MPI_ROOT}/bin:${PATH}',
                  'GASNET_CONDUIT': 'ibv'}
}

job_ocr_verify_kernel_sw_gasnet = {
    'name': 'ocr-verify-kernel-sw-gasnet',
    'depends': ('ocr-run-kernel-sw-gasnet',),
    'jobtype': 'ocr-verify-kernel-local',
    'run-args': '-w -c 1',
    'sandbox': ('inherit0',),
    'env-vars': { 'WORKLOAD_EXEC': '${JJOB_SHARED_HOME}/xstack/ocr/examples/smith-waterman/install/x86-gasnet'}
}

# job_ocr_build_kernel_sw_tgemul = {
#     'name': 'ocr-build-kernel-sw-tgemul',
#     'depends': ('ocr-build-tg-x86',),
#     'jobtype': 'ocr-build-kernel',
#     'run-args': 'smith-waterman tg-x86',
#     'sandbox': ('inherit0',),
#     'env-vars': { 'APPS_LIBS_ROOT': '${JJOB_SHARED_HOME}/xstack/apps/libs/x86',
#                   'WORKLOAD_SRC': '${JJOB_INITDIR_XSTACK}/ocr/examples/smith-waterman',
#                   'WORKLOAD_BUILD_ROOT': '${JJOB_PRIVATE_HOME}/xstack/ocr/examples/smith-waterman/build',
#                   'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/xstack/ocr/examples/smith-waterman/install'}
# }

# job_ocr_run_kernel_sw_tgemul = {
#     'name': 'ocr-run-kernel-sw-tgemul',
#     'depends': ('ocr-build-kernel-sw-tgemul',),
#     'jobtype': 'ocr-run-kernel-local',
#     'run-args': 'smith-waterman tg-x86',
#     'sandbox': ('inherit0',),
#     'env-vars': { 'APPS_LIBS_ROOT': '${JJOB_SHARED_HOME}/xstack/apps/libs/x86',
#                   'WORKLOAD_ARGS': '100 100 ${JJOB_SHARED_HOME}/xstack/apps/smithwaterman/datasets/string1-medium-large.txt  ${JJOB_SHARED_HOME}/xstack/apps/smithwaterman/datasets/string2-medium-large.txt ${JJOB_SHARED_HOME}/xstack/apps/smithwaterman/datasets/score-medium-large.txt',
#                   'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/xstack/ocr/examples/smith-waterman/install' }
# }

# job_ocr_verify_kernel_sw_tgemul = {
#     'name': 'ocr-verify-kernel-sw-tgemul',
#     'depends': ('ocr-run-kernel-sw-tgemul',),
#     'jobtype': 'ocr-verify-kernel-local',
#     'run-args': '-w -c 1',
#     'sandbox': ('inherit0',),
#     'env-vars': { 'WORKLOAD_EXEC': '${JJOB_SHARED_HOME}/xstack/ocr/examples/smith-waterman/install/tg-x86'}
# }

job_ocr_build_kernel_sw_tg = {
    'name': 'ocr-build-kernel-sw-tg',
    'depends': ('ocr-build-builder-ce', 'ocr-build-builder-xe',
                'ocr-build-tg-ce', 'ocr-build-tg-xe'),
    'jobtype': 'ocr-build-kernel-tg',
    'run-args': 'smith-waterman tg',
    'sandbox': ('inherit0',),
    'env-vars': { 'APPS_LIBS_ROOT': '${JJOB_SHARED_HOME}/xstack/apps/libs/tg',
                  'WORKLOAD_SRC': '${JJOB_INITDIR_XSTACK}/ocr/examples/smith-waterman',
                  'WORKLOAD_BUILD_ROOT': '${JJOB_PRIVATE_HOME}/xstack/ocr/examples/smith-waterman/build',
                  'WORKLOAD_ARGS': '10 10 ${JJOB_SHARED_HOME}/xstack/apps/smithwaterman/datasets/string1-medium.txt ${JJOB_SHARED_HOME}/xstack/apps/smithwaterman/datasets/string2-medium.txt ${JJOB_SHARED_HOME}/xstack/apps/smithwaterman/datasets/score-medium.txt',
                  'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/xstack/ocr/examples/smith-waterman/install'}
}

job_ocr_run_kernel_sw_tg = {
    'name': 'ocr-run-kernel-sw-tg',
    'depends': ('ocr-build-kernel-sw-tg',),
    'jobtype': 'ocr-run-kernel-remote-tg',
    'run-args': 'fft tg',
    'param-args': '-c ${WORKLOAD_INSTALL_ROOT}/tg/config.cfg',
    'sandbox': ('inherit0',),
    'env-vars': { 'APPS_LIBS_ROOT': '${JJOB_SHARED_HOME}/xstack/apps/libs/tg',
                  'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/xstack/ocr/examples/smith-waterman/install'}
}

job_ocr_verify_kernel_sw_tg = {
    'name': 'ocr-verify-kernel-sw-tg',
    'depends': ('ocr-run-kernel-sw-tg',),
    'jobtype': 'ocr-verify-kernel-remote',
    'run-args': '-w -c 1',
    'sandbox': ('inherit0',),
    'env-vars': { 'WORKLOAD_EXEC': '${JJOB_SHARED_HOME}/xstack/ocr/examples/smith-waterman/install/tg'}
}
# Cholesky

job_ocr_build_kernel_cholesky_x86 = {
    'name': 'ocr-build-kernel-cholesky-x86',
    'depends': ('ocr-build-x86',),
    'jobtype': 'ocr-build-kernel',
    'run-args': 'cholesky x86',
    'sandbox': ('inherit0',),
    'env-vars': { 'APPS_LIBS_ROOT': '${JJOB_SHARED_HOME}/xstack/apps/libs/x86',
                  'WORKLOAD_SRC': '${JJOB_INITDIR_XSTACK}/ocr/examples/cholesky',
                  'WORKLOAD_BUILD_ROOT': '${JJOB_PRIVATE_HOME}/xstack/ocr/examples/cholesky/build',
                  'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/xstack/ocr/examples/cholesky/install'}
}

job_ocr_run_kernel_cholesky_x86 = {
    'name': 'ocr-run-kernel-cholesky-x86',
    'depends': ('ocr-build-kernel-cholesky-x86',),
    'jobtype': 'ocr-run-kernel-local',
    'run-args': 'cholesky x86',
    'sandbox': ('inherit0',),
    'env-vars': { 'APPS_LIBS_ROOT': '${JJOB_SHARED_HOME}/xstack/apps/libs/x86',
                  'WORKLOAD_ARGS': '--ds 1000 --ts 50 --fi ${JJOB_SHARED_HOME}/xstack/apps/cholesky/datasets/m_1000.in',
                  'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/xstack/ocr/examples/cholesky/install' }
}

job_ocr_verify_kernel_cholesky_x86 = {
    'name': 'ocr-verify-kernel-cholesky-x86',
    'depends': ('ocr-run-kernel-cholesky-x86',),
    'jobtype': 'ocr-verify-diff',
    'run-args': '${WORKLOAD_EXEC}/cholesky.out.txt ${JJOB_SHARED_HOME}/xstack/apps/cholesky/datasets/cholesky_out_1000.txt',
    'sandbox': ('inherit0',),
    'env-vars': { 'WORKLOAD_EXEC': '${JJOB_SHARED_HOME}/xstack/ocr/examples/cholesky/install/x86' }
}

job_ocr_build_kernel_cholesky_mpi = {
    'name': 'ocr-build-kernel-cholesky-mpi',
    'depends': ('ocr-build-x86-mpi',),
    'jobtype': 'ocr-build-kernel',
    'run-args': 'cholesky x86-mpi',
    'sandbox': ('inherit0',),
    'env-vars': { 'APPS_LIBS_ROOT': '${JJOB_SHARED_HOME}/xstack/apps/libs/x86',
                  'WORKLOAD_SRC': '${JJOB_INITDIR_XSTACK}/ocr/examples/cholesky',
                  'WORKLOAD_BUILD_ROOT': '${JJOB_PRIVATE_HOME}/xstack/ocr/examples/cholesky/build',
                  'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/xstack/ocr/examples/cholesky/install'}
}

job_ocr_run_kernel_cholesky_mpi = {
    'name': 'ocr-run-kernel-cholesky-mpi',
    'depends': ('ocr-build-kernel-cholesky-mpi',),
    'jobtype': 'ocr-run-kernel-local',
    'run-args': 'cholesky x86-mpi',
    'sandbox': ('inherit0',),
    'env-vars': { 'PATH': '/opt/intel/tools/impi/5.1.1.109/intel64/bin:${PATH}',
                  'APPS_LIBS_ROOT': '${JJOB_SHARED_HOME}/xstack/apps/libs/x86',
                  'WORKLOAD_ARGS': '--ds 1000 --ts 50 --fi ${JJOB_SHARED_HOME}/xstack/apps/cholesky/datasets/m_1000.in',
                  'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/xstack/ocr/examples/cholesky/install' }
}

job_ocr_verify_kernel_cholesky_mpi = {
    'name': 'ocr-verify-kernel-cholesky-mpi',
    'depends': ('ocr-run-kernel-cholesky-mpi',),
    'jobtype': 'ocr-verify-diff',
    'run-args': '${WORKLOAD_EXEC}/cholesky.out.txt ${JJOB_SHARED_HOME}/xstack/apps/cholesky/datasets/cholesky_out_1000.txt',
    'sandbox': ('inherit0',),
    'env-vars': { 'WORKLOAD_EXEC': '${JJOB_SHARED_HOME}/xstack/ocr/examples/cholesky/install/x86-mpi' }
}

job_ocr_build_kernel_cholesky_gasnet = {
    'name': 'ocr-build-kernel-cholesky-gasnet',
    'depends': ('ocr-build-x86-gasnet',),
    'jobtype': 'ocr-build-kernel',
    'run-args': 'cholesky x86-gasnet',
    'sandbox': ('inherit0',),
    'env-vars': { 'APPS_LIBS_ROOT': '${JJOB_SHARED_HOME}/xstack/apps/libs/x86',
                  'WORKLOAD_SRC': '${JJOB_INITDIR_XSTACK}/ocr/examples/cholesky',
                  'WORKLOAD_BUILD_ROOT': '${JJOB_PRIVATE_HOME}/xstack/ocr/examples/cholesky/build',
                  'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/xstack/ocr/examples/cholesky/install',
                  'MPI_ROOT': '/opt/intel/tools/impi/5.1.1.109/intel64',
                  'GASNET_ROOT': '/opt/rice/GASNet/1.24.0-impi',
                  'PATH': '${GASNET_ROOT}/bin:${MPI_ROOT}/bin:${PATH}',
                  'GASNET_CONDUIT': 'ibv',
                  'GASNET_TYPE': 'par',
                  'GASNET_EXTRA_LIBS': '-L/usr/lib64 -lrt -libverbs',
                  'CC': 'mpicc', # gasnet built with mpi
                  }
}

job_ocr_run_kernel_cholesky_gasnet = {
    'name': 'ocr-run-kernel-cholesky-gasnet',
    'depends': ('ocr-build-kernel-cholesky-gasnet',),
    'jobtype': 'ocr-run-kernel-local',
    'run-args': 'cholesky x86-gasnet',
    'sandbox': ('inherit0',),
    'env-vars': { 'PATH': '/opt/intel/tools/impi/5.1.1.109/intel64/bin:${PATH}',
                  'APPS_LIBS_ROOT': '${JJOB_SHARED_HOME}/xstack/apps/libs/x86',
                  'WORKLOAD_ARGS': '--ds 1000 --ts 50 --fi ${JJOB_SHARED_HOME}/xstack/apps/cholesky/datasets/m_1000.in',
                  'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/xstack/ocr/examples/cholesky/install',
                  'GASNET_ROOT': '/opt/rice/GASNet/1.24.0-impi',
                  'PATH': '${GASNET_ROOT}/bin:${MPI_ROOT}/bin:${PATH}',
                  'GASNET_CONDUIT': 'ibv'}
}

job_ocr_verify_kernel_cholesky_gasnet = {
    'name': 'ocr-verify-kernel-cholesky-gasnet',
    'depends': ('ocr-run-kernel-cholesky-gasnet',),
    'jobtype': 'ocr-verify-diff',
    'run-args': '${WORKLOAD_EXEC}/cholesky.out.txt ${JJOB_SHARED_HOME}/xstack/apps/cholesky/datasets/cholesky_out_1000.txt',
    'sandbox': ('inherit0',),
    'env-vars': { 'WORKLOAD_EXEC': '${JJOB_SHARED_HOME}/xstack/ocr/examples/cholesky/install/x86-gasnet' }
}

# job_ocr_build_kernel_cholesky_tgemul = {
#     'name': 'ocr-build-kernel-cholesky-tgemul',
#     'depends': ('ocr-build-tg-x86',),
#     'jobtype': 'ocr-build-kernel',
#     'run-args': 'cholesky tg-x86',
#     'sandbox': ('inherit0',),
#     'env-vars': { 'APPS_LIBS_ROOT': '${JJOB_SHARED_HOME}/xstack/apps/libs/x86',
#                   'WORKLOAD_SRC': '${JJOB_INITDIR_XSTACK}/ocr/examples/cholesky',
#                   'WORKLOAD_BUILD_ROOT': '${JJOB_PRIVATE_HOME}/xstack/ocr/examples/cholesky/build',
#                   'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/xstack/ocr/examples/cholesky/install'}
# }

# job_ocr_run_kernel_cholesky_tgemul = {
#     'name': 'ocr-run-kernel-cholesky-tgemul',
#     'depends': ('ocr-build-kernel-cholesky-tgemul',),
#     'jobtype': 'ocr-run-kernel-local',
#     'run-args': 'cholesky tg-x86',
#     'sandbox': ('inherit0',),
#     'env-vars': { 'APPS_LIBS_ROOT': '${JJOB_SHARED_HOME}/xstack/apps/libs/x86',
#                   'WORKLOAD_ARGS': '--ds 1000 --ts 50 --fi ${JJOB_SHARED_HOME}/xstack/apps/cholesky/datasets/m_1000.in',
#                   'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/xstack/ocr/examples/cholesky/install' }
# }

# job_ocr_verify_kernel_cholesky_tgemul = {
#     'name': 'ocr-verify-kernel-cholesky-tgemul',
#     'depends': ('ocr-run-kernel-cholesky-tgemul',),
#     'jobtype': 'ocr-verify-diff',
#     'run-args': '${WORKLOAD_EXEC}/cholesky.out.txt ${JJOB_SHARED_HOME}/xstack/apps/cholesky/datasets/cholesky_out_1000.txt',
#     'sandbox': ('inherit0',),
#     'env-vars': { 'WORKLOAD_EXEC': '${JJOB_SHARED_HOME}/xstack/ocr/examples/cholesky/install/tg-x86' }
# }

job_ocr_build_kernel_cholesky_tg = {
    'name': 'ocr-build-kernel-cholesky-tg',
    'depends': ('ocr-build-builder-ce', 'ocr-build-builder-xe',
                'ocr-build-tg-ce', 'ocr-build-tg-xe'),
    'jobtype': 'ocr-build-kernel-tg',
    'run-args': 'cholesky tg',
    'sandbox': ('inherit0',),
    'env-vars': { 'APPS_LIBS_ROOT': '${JJOB_SHARED_HOME}/xstack/apps/libs/tg',
                  'WORKLOAD_SRC': '${JJOB_INITDIR_XSTACK}/ocr/examples/cholesky',
                  'WORKLOAD_BUILD_ROOT': '${JJOB_PRIVATE_HOME}/xstack/ocr/examples/cholesky/build',
                  'WORKLOAD_ARGS': '--ds 50 --ts 10 --fi ${JJOB_SHARED_HOME}/xstack/apps/cholesky/datasets/m_50.in',
                  'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/xstack/ocr/examples/cholesky/install'}
}

job_ocr_run_kernel_cholesky_tg = {
    'name': 'ocr-run-kernel-cholesky-tg',
    'depends': ('ocr-build-kernel-cholesky-tg',),
    'jobtype': 'ocr-run-kernel-remote-tg',
    'run-args': 'cholesky tg',
    'param-args': '-c ${WORKLOAD_INSTALL_ROOT}/tg/config.cfg',
    'sandbox': ('inherit0',),
    'env-vars': { 'APPS_LIBS_ROOT': '${JJOB_SHARED_HOME}/xstack/apps/libs/tg',
                  'WORKLOAD_ARGS': '--ds 50 --ts 10 --fi ${JJOB_SHARED_HOME}/xstack/apps/cholesky/datasets/m_50.in',
                  'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/xstack/ocr/examples/cholesky/install'}
}

job_ocr_verify_kernel_cholesky_tg = {
    'name': 'ocr-verify-kernel-cholesky-tg',
    'depends': ('ocr-run-kernel-cholesky-tg',),
    'jobtype': 'ocr-verify-diff',
    'run-args': '${WORKLOAD_EXEC}/cholesky.out.txt ${JJOB_SHARED_HOME}/xstack/apps/cholesky/datasets/cholesky_out_50.txt',
    'sandbox': ('inherit0',),
    'env-vars': { 'WORKLOAD_EXEC': '${JJOB_SHARED_HOME}/xstack/ocr/examples/cholesky/install/tg' }
}

