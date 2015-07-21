#!/bin/bash

if [ $# -ne 2 ]; then
    echo "Usage <sub-directory> <arch>"
    exit 1
fi

# If APSS_MAKEFILE is not set, use WORKLOAD_SRC/Makefile
APPS_MAKEFILE=${APPS_MAKEFILE:-${WORKLOAD_SRC}/Makefile}

echo "Building kernel '$1' for architecture '$2'"
RUN_JENKINS=buildApp make -f ${APPS_MAKEFILE}.$2 all install
RETURN_CODE=$?

if [ $RETURN_CODE -eq 0 ]; then
    # Copy the makefile over
    # This is to enable running remote jobs from the install directory
    cp ${APPS_MAKEFILE}.$2 ${WORKLOAD_INSTALL_ROOT}/$2/
    echo "**** Build SUCCESS ****"
else
    echo "**** Build FAILURE ****"
fi

exit $RETURN_CODE
