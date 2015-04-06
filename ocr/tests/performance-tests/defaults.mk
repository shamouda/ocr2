# Common
# How many DBs to create at a time?
DB_NBS ?= 32
# Number of iterations to run
NB_ITERS ?= 50000
# How many (EDTs/Events) to create at once
NB_INSTANCES ?= 99999

# Event Tests
# No of deps each EDT has, to start with
DRIVER_DEPV ?= 0
# No of params each EDT has
DRIVER_PARAMC ?= 2
# Fan out of EDTs
FAN_OUT ?= 1000
# What type of EDT to create (finish/none)
DRIVER_PROP ?= EDT_PROP_NONE

# Run Information set by the driver
# Nb of OCR workers
NB_WORKERS ?= 1
# Nb of nodes
NB_NODES ?= 1

# DB Tests
# Size of each DB
DB_SZ ?= 4

#EDT Tests
# Fan out for non-leaf EDTs in hierarchical tests
NODE_FANOUT ?= 2
# Fan out for leaf EDTs in hierarchical tests
LEAF_FANOUT ?= 100
# Tree depth for hierarchial tests
TREE_DEPTH ?= 8

# When creating DBs, how many units should each be?
DB_NB_ELT ?= 10
# Size of each unit in the above
DB_TYPE ?= u64
# Number of params for each EDT
PARAMC_SIZE ?= 0

C_DEFINES := -DDB_NBS=$(DB_NBS) -DNB_ITERS=$(NB_ITERS) -DNB_INSTANCES=$(NB_INSTANCES)\
             -DDRIVER_DEPV=$(DRIVER_DEPV) -DDRIVER_PARAMC=$(DRIVER_PARAMC) -DFAN_OUT=$(FAN_OUT)\
             -DDRIVER_PROP=$(DRIVER_PROP) -DDB_SZ=$(DB_SZ) -DNODE_FANOUT=$(NODE_FANOUT)\
             -DLEAF_FANOUT=$(LEAF_FANOUT) -DTREE_DEPTH=$(TREE_DEPTH) -DDB_NB_ELT=$(DB_NB_ELT)\
             -DDB_TYPE=$(DB_TYPE) -DPARAMC_SIZE=$(PARAMC_SIZE) \
             -DNB_WORKERS=$(NB_WORKERS) -DNB_NODES=$(NB_NODES)
