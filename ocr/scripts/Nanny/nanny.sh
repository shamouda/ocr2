#!/bin/bash

CUR=$PWD
OCR_CONFIG_NANNY=${CUR}/nanny.cfg

# Generate a configuration file suitable for debugging:
#
# - Use system malloc instead of memory-allocators
#   => Done by modifying common.mk
# - Single Worker
# - Use GUID provider that tracks deallocations
#   => COUNTED_MAP + build flag
# - Use simplest scheduler (no hints, no heuristics)
#   => Just rely on regular scheduler for now

../Configs/config-generator.py --threads 1 --guid COUNTED_MAP --target x86 --platform X86 --dbtype Lockable --output ${OCR_CONFIG_NANNY} --remove-destination

export CFLAGS="${CFLAGS} -DNANNYMODE_SYSTEM_MALLOC"

cd ../../build/x86; make && make install && echo "Nanny mode build successful !"; cd -;
