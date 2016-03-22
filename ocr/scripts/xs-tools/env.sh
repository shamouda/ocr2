#!/bin/bash

# Look for default XST config
if [[ -z "${XST_CONF}" ]]; then
    XST_CONF=${HOME}/xst.conf
fi

# Source
. ${XST_CONF}

# Default Environment variables
export MAKE_THREADS=${MAKE_THREADS-16}
