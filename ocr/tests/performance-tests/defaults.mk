# Common
# How many DBs to create at a time?
DB_NBS ?= 32
NB_EVT_COUNTED_DEPS ?= 4
# Number of iterations to run
NB_ITERS ?= 1000
# How many (EDTs/Events) to create at once
NB_INSTANCES ?= 1000

# Event Tests
# Nb of paramc each EDT has
PARAMC_SZ ?= 2
# Nb of deps each EDT has
DEPV_SZ ?= 0
# Fan out of EDTs
FAN_OUT ?= 100

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
LEAF_FANOUT ?= 10
# Tree depth for hierarchial tests
TREE_DEPTH ?= 8

# When creating DBs, how many units should each be?
DB_NB_ELT ?= 10
# Size of each unit in the above
DB_TYPE ?= u64

C_DEFINES := -DENABLE_EXTENSION_AFFINITY -DENABLE_EXTENSION_RTITF -DENABLE_EXTENSION_PARAMS_EVT -DENABLE_EXTENSION_COUNTED_EVT \
             -DDB_NBS=$(DB_NBS) -DNB_EVT_COUNTED_DEPS=$(NB_EVT_COUNTED_DEPS) -DNB_ITERS=$(NB_ITERS) -DNB_INSTANCES=$(NB_INSTANCES)\
             -DDEPV_SZ=$(DEPV_SZ) -DPARAMC_SZ=$(PARAMC_SZ) -DFAN_OUT=$(FAN_OUT)\
             -DDB_SZ=$(DB_SZ) -DNODE_FANOUT=$(NODE_FANOUT)\
             -DLEAF_FANOUT=$(LEAF_FANOUT) -DTREE_DEPTH=$(TREE_DEPTH) -DDB_NB_ELT=$(DB_NB_ELT)\
             -DDB_TYPE=$(DB_TYPE) -DNB_WORKERS=$(NB_WORKERS) -DNB_NODES=$(NB_NODES) -DOCR_TYPE_H=$(OCR_TYPE).h
