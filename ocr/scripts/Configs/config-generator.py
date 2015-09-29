#!/usr/bin/python

import argparse
import multiprocessing
import os.path
import sys

# README First
# This script currently generates only OCR-HC configs
# WIP to extend it to:
# 1. Target FSim on X86
# 2. Read FSim machine config to spit out OCR-FSim configs

parser = argparse.ArgumentParser(description='Generate an OCR config file.')
parser.add_argument('--guid', dest='guid', default='PTR', choices=['PTR', 'COUNTED_MAP', 'LABELED'],
                   help='guid type to use (default: PTR)')
parser.add_argument('--platform', dest='platform', default='X86', choices=['X86', 'FSIM'],
                   help='platform type to use (default: X86)')
parser.add_argument('--target', dest='target', default='x86', choices=['x86', 'fsim', 'mpi', 'gasnet'],
                   help='target type to use (default: X86)')
parser.add_argument('--threads', dest='threads', type=int, default=4,
                   help='number of threads available to OCR (default: 4)')
parser.add_argument('--binding', dest='binding', default='none', choices=['none', 'seq', 'spread'],
                   help='perform thread binding (default: no binding)')
parser.add_argument('--alloc', dest='alloc', default='32',
                   help='size (in MB) of memory available for app use (default: 32)')
parser.add_argument('--alloctype', dest='alloctype', default='mallocproxy', choices=['mallocproxy', 'tlsf', 'simple'],
                   help='type of allocator to use (default: mallocproxy)')
parser.add_argument('--dbtype', dest='dbtype', default='Lockable', choices=['Lockable', 'Regular'],
                   help='type of datablocks to use (default: Lockable)')
parser.add_argument('--output', dest='output', default='default.cfg',
                   help='config output filename (default: default.cfg)')
parser.add_argument('--remove-destination', dest='rmdest', action='store_true',
                   help='remove output file if exists (default: no)')


args = parser.parse_args()
guid = args.guid
platform = args.platform
target = args.target.upper()
if target == 'GASNET':
    target = 'GASNet'
threads = args.threads
binding = args.binding
alloc = args.alloc
alloctype = args.alloctype
dbtype = args.dbtype
outputfilename = args.output
rmdest = args.rmdest

def GenerateVersion(output):
    version = "1.0.1"
    output.write("[General]\n\tversion\t=\t%s\n\n" % (version))
    output.write("\n#======================================================\n")

def GenerateGuid(output, guid):
    output.write("[GuidType0]\n\tname\t=\t%s\n\n" % (guid))
    output.write("[GuidInst0]\n\tid\t=\t%d\n\ttype\t=\t%s\n" % (0, guid))
    output.write("\n#======================================================\n")

def GeneratePd(output, pdtype, dbtype, threads):
    output.write("[PolicyDomainType0]\n\tname\t=\t%s\n\n" % (pdtype))
    output.write("[PolicyDomainInst0]\n")
    output.write("\tid\t\t\t=\t0\n")
    output.write("\ttype\t\t\t=\t%s\n" % (pdtype))
    output.write("\tworker\t\t\t=\t0-%d\n" % (threads-1))
    output.write("\tscheduler\t\t=\t0\n")
    output.write("\tallocator\t\t=\t0\n")
    if pdtype == 'HCDist':
        output.write("\tcommapi\t\t\t=\t0-%d\n" % (threads-1))
    else:
        output.write("\tcommapi\t\t\t=\t0\n")
    output.write("\tguid\t\t\t=\t0\n")
    output.write("\tparent\t\t\t=\t0\n")
    output.write("\tlocation\t\t=\t0\n")
    pdtype = "HC"
    output.write("\ttaskfactory\t\t=\t%s\n" % (pdtype))
    output.write("\ttasktemplatefactory\t=\t%s\n" % (pdtype))
    output.write("\tdatablockfactory\t=\t%s\n" % (dbtype))
    output.write("\teventfactory\t\t=\t%s\n\n" % (pdtype))

def GenerateCommon(output, pdtype, dbtype):
    output.write("[TaskType0]\n\tname=\t%s\n\n" % (pdtype))
    output.write("[TaskTemplateType0]\n\tname=\t%s\n\n" % (pdtype))
    output.write("[DataBlockType0]\n\tname=\t%s\n\n" % (dbtype))
    output.write("[EventType0]\n\tname=\t%s\n\n" % (pdtype))
    output.write("\n#======================================================\n")

def GenerateMem(output, size, count, alloctype):
    output.write("[MemPlatformType0]\n\tname\t=\t%s\n" % ("malloc"))
    output.write("[MemPlatformInst0]\n")
    output.write("\tid\t=\t0\n")
    output.write("\ttype\t=\t%s\n" % ("malloc"))
    output.write("\tsize\t=\t%d\n" % (int(size*1.05)))
    output.write("\n#======================================================\n")
    output.write("[MemTargetType0]\n\tname\t=\t%s\n" % ("shared"))
    output.write("[MemTargetInst0]\n")
    output.write("\tid\t=\t0\n")
    output.write("\ttype\t=\t%s\n" % ("shared"))
    output.write("\tsize\t=\t%d\n" % (int(size*1.05)))
    output.write("\tmemplatform\t=\t0\n")
    output.write("\n#======================================================\n")
    output.write("[AllocatorType0]\n\tname\t=\t%s\n" % (alloctype))
    output.write("[AllocatorInst0]\n")
    output.write("\tid\t=\t0\n")
    output.write("\ttype\t=\t%s\n" % (alloctype))
    output.write("\tsize\t=\t%d\n" % (size))
    output.write("\tmemtarget\t=\t0\n")
    output.write("\n#======================================================\n")

def GenerateComm(output, comms, pdtype, threads):
    output.write("\n#======================================================\n")
    if pdtype == 'HCDist':
        output.write("[CommApiType0]\n\tname\t=\t%s\n" % ("Simple"))
        output.write("[CommApiInst0]\n")
        output.write("\tid\t=\t0\n")
        output.write("\ttype\t=\t%s\n" % ("Simple"))
        output.write("\tcommplatform\t=\t0\n")
        output.write("[CommApiType1]\n\tname\t=\t%s\n" % ("Delegate"))
        output.write("[CommApiInst1]\n")
        output.write("\tid\t=\t1-%d\n" % (threads-1))
        output.write("\ttype\t=\t%s\n" % ("Delegate"))
        output.write("\tcommplatform\t=\t1-%d\n" % (threads-1))
        output.write("\n#======================================================\n")
        output.write("[CommPlatformType0]\n\tname\t=\t%s\n" % ("None"))
        output.write("[CommPlatformInst0]\n")
        output.write("\tid\t=\t1-%d\n" % (threads-1))
        output.write("\ttype\t=\t%s\n" % ("None"))
        output.write("[CommPlatformType1]\n\tname\t=\t%s\n" % (comms))
        output.write("[CommPlatformInst1]\n")
        output.write("\tid\t=\t0\n")
        output.write("\ttype\t=\t%s\n" % (comms))
    else:
        output.write("[CommPlatformType0]\n\tname\t=\t%s\n" % ("None"))
        output.write("[CommPlatformInst0]\n")
        output.write("\tid\t=\t0\n")
        output.write("\ttype\t=\t%s\n" % ("None"))
        output.write("\n#======================================================\n")
        output.write("[CommApiType0]\n\tname\t=\t%s\n" % ("Handleless"))
        output.write("[CommApiInst0]\n")
        output.write("\tid\t=\t0\n")
        output.write("\ttype\t=\t%s\n" % ("Handleless"))
        output.write("\tcommplatform\t=\t0\n")
        output.write("\n#======================================================\n")
        output.write("[CommPlatformType0]\n\tname\t=\t%s\n" % ("None"))
        output.write("[CommPlatformInst0]\n")
        output.write("\tid\t=\t0\n")
        output.write("\ttype\t=\t%s\n" % ("None"))
    output.write("\n#======================================================\n")

def GenerateComp(output, pdtype, threads, binding, schedtype):
    output.write("[CompPlatformType0]\n\tname\t=\t%s\n" % ("pthread"))
    output.write("\tstacksize\t=\t0\n")
    output.write("[CompPlatformInst0]\n")
    output.write("\tid\t=\t0-%d\n" % (threads-1))
    output.write("\ttype\t=\t%s\n" % ("pthread"))
    output.write("\tstacksize\t=\t0\n")
    if binding != 'none':
        output.write("\tbinding\t=\t")
        if binding == 'seq':
            for i in range(0, threads-1):
                output.write("%d," % i)
            output.write("%d\n" % (threads-1))
        else: # binding == spread
            count = 0
            for i in range(0, threads, 2):
                output.write("%d" % i)
                count = count+1
                if count != threads:
                    output.write(",")
                else:
                    output.write("\n")
            for i in range(1, threads, 2):
                output.write("%d" % i)
                count = count+1
                if count != threads:
                    output.write(",")
                else:
                    output.write("\n")
    output.write("\n#======================================================\n")
    output.write("[CompTargetType0]\n\tname\t=\t%s\n" % ("PASSTHROUGH"))
    output.write("[CompTargetInst0]\n")
    output.write("\tid\t=\t0-%d\n" % (threads-1))
    output.write("\ttype\t=\t%s\n" % ("PASSTHROUGH"))
    output.write("\tcompplatform\t=\t0-%d\n" % (threads-1))
    output.write("\n#======================================================\n")
    masterWorkerType = "HC_COMM" if (pdtype == 'HCDist') else "HC"
    output.write("[WorkerType0]\n\tname\t=\t%s\n" % (masterWorkerType))
    output.write("[WorkerInst0]\n")
    output.write("\tid\t=\t0\n")
    output.write("\ttype\t=\t%s\n" % (masterWorkerType))
    output.write("\tworkertype\t=\tmaster\n")
    output.write("\tcomptarget\t=\t0\n")
    if threads > 1:
        if (pdtype == 'HCDist'): # Need a second type for distributed
            output.write("[WorkerType1]\n\tname\t=\tHC\n")
        output.write("[WorkerInst1]\n")
        output.write("\tid\t=\t1-%d\n" % (threads-1))
        output.write("\ttype\t=\tHC\n")
        output.write("\tworkertype\t=\tslave\n")
        output.write("\tcomptarget\t=\t1-%d\n" % (threads-1))
    output.write("\n#======================================================\n")
    output.write("[WorkPileType0]\n\tname\t=\t%s\n" % ("HC"))
    output.write("[WorkPileInst0]\n")
    output.write("\tid\t=\t0-%d\n" % (threads-1))
    output.write("\ttype\t=\tHC\n")
    output.write("\n#======================================================\n")
    if (pdtype == 'HCDist'):
        output.write("[SchedulerObjectType0]\n")
        output.write("\tname\t=\tNULL\n")
        output.write("[SchedulerObjectInst0]\n")
        output.write("\tid\t=\t0\n")
        output.write("\ttype\t=\tNULL\n")
        output.write("\n#======================================================\n")
        output.write("[SchedulerHeuristicType0]\n\tname\t=\tNULL\n")
        output.write("[SchedulerHeuristicInst0]\n")
        output.write("\tid\t=\t0\n")
        output.write("\ttype\t=\tNULL\n")
        output.write("\n#======================================================\n")
    else:
        output.write("[SchedulerObjectType0]\n")
        output.write("\tname\t=\t%s\n" % ("DOMAIN"))
        output.write("[SchedulerObjectType1]\n")
        output.write("\tname\t=\t%s\n" % ("DBNODE"))
        output.write("[SchedulerObjectType2]\n")
        output.write("\tname\t=\t%s\n" % ("WST"))
        output.write("\tkind\t=\t%s\n" % ("root"))
        output.write("[SchedulerObjectInst0]\n")
        output.write("\tid\t=\t0\n")
        output.write("\ttype\t=\t%s\n" % ("WST"))
        output.write("[SchedulerObjectType3]\n")
        output.write("\tname\t=\t%s\n" % ("DEQ"))
        output.write("[SchedulerObjectInst1]\n")
        output.write("\tid\t=\t1\n")
        output.write("\ttype\t=\t%s\n" % ("DEQ"))
        output.write("[SchedulerObjectType4]\n")
        output.write("\tname\t=\t%s\n" % ("LIST"))
        output.write("[SchedulerObjectType5]\n")
        output.write("\tname\t=\t%s\n" % ("MAP"))
        output.write("\n#======================================================\n")
        output.write("[SchedulerHeuristicType0]\n\tname\t=\t%s\n" % ("HC"))
        output.write("[SchedulerHeuristicInst0]\n")
        output.write("\tid\t=\t0\n")
        output.write("\ttype\t=\t%s\n" % ("HC"))
        output.write("\n#======================================================\n")
    output.write("[SchedulerType0]\n\tname\t=\t%s\n" % (schedtype))
    output.write("[SchedulerInst0]\n")
    output.write("\tid\t=\t0\n")
    output.write("\ttype\t=\t%s\n" % (schedtype))
    output.write("\tworkpile\t=\t0-%d\n" % (threads-1))
    output.write("\tworkeridfirst\t=\t0\n")
    output.write("\tschedulerObject\t=\t0\n")
    output.write("\tschedulerHeuristic\t=\t0\n")
    output.write("\n#======================================================\n")

def GenerateConfig(filehandle, guid, platform, target, threads, binding, alloc, alloctype, dbtype):
    filehandle.write("# Generated by python script, modify as desired.\n")
    filehandle.write("# To use this config file, set OCR_CONFIG to the filename prior to running the OCR program.\n\n")
    GenerateVersion(filehandle)
    GenerateGuid(filehandle, guid)
    if target=='X86':
        GeneratePd(filehandle, "HC", dbtype, threads)
        GenerateCommon(filehandle, "HC", dbtype)
        GenerateMem(filehandle, alloc, 1, alloctype)
        GenerateComm(filehandle, "null", "HC", threads)
        GenerateComp(filehandle, "HC", threads, binding, "COMMON")
    elif target=='FSIM':
        GeneratePd(filehandle, "CE", dbtype, 1)
        GeneratePd(filehandle, "XE", dbtype, threads)
        GenerateCommon(filehandle, "HC", dbtype)
        GenerateMem(filehandle, alloc, 1, alloctype)
    elif (target=='MPI') or (target=='GASNet'):
        # catch default value errors for distributed
        if dbtype != 'Lockable':
            print 'error: target ', target, ' only supports Lockable datablocks; received ', dbtype
            os.unlink(filehandle.name)
            raise
        if guid != 'COUNTED_MAP' and guid != 'LABELED':
            print 'error: target ', target, ' only supports counted-map guid provider; received ', guid
            os.unlink(filehandle.name)
            raise
        pdtype="HCDist"
        GeneratePd(filehandle, pdtype, dbtype, threads)
        #Intentionally use "HC" here
        GenerateCommon(filehandle, "HC", dbtype)
        GenerateMem(filehandle, alloc, 1, alloctype)
        GenerateComm(filehandle, target, pdtype, threads)
        GenerateComp(filehandle, pdtype, threads, binding, "HC_COMM_DELEGATE")
    else:
        print 'Target ', target, ' unsupported'
        sys.exit(0)


def GetString(prompt, default):
    prompt = "%s [%s] " % (prompt, default)
    value = raw_input(prompt)
    if value == '':
        value = default
    if value == str(value):
        return value
    return int(value)


def CheckValue(value, valid):
    if str(value) not in str(valid):
        print 'Warning ', value, '; outside the range of: ', valid

if target=='X86' and platform=='FSIM':
    print 'Incorrect values for platform & target (possibly switched?)'
    raise

if platform=='X86' or platform=='FSIM':
    max = multiprocessing.cpu_count()
    CheckValue(threads, range(1,max+1))

    alloc = int(alloc)*1048576

    if os.path.isfile(outputfilename) and (not rmdest):
        print 'Output file ', outputfilename, ' already exists'
        sys.exit(0)
    else:
        with open(outputfilename, "w") as filehandle:
            GenerateConfig(filehandle, guid, platform, target, threads, binding, alloc, alloctype, dbtype)
else:
    print 'Platform ', platform, ' and target ', target, ' unsupported'
    raise
