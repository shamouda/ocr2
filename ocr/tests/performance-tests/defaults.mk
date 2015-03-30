# Common
DB_NBS ?= 32                   # How many DBs to create at a time?
NB_ITERS ?= 50000              # Number of iterations to run
NB_INSTANCES ?= 99999          # How many (EDTs/Events) to create at once

# Event Tests
DRIVER_DEPV ?= 0                  # No of deps each EDT has, to start with
DRIVER_PARAMC ?= 2                # No of params each EDT has
FAN_OUT ?= 1000                   # Fan out of EDTs
DRIVER_PROP ?= EDT_PROP_NONE      # What type of EDT to create (finish/none)

# DB Tests
DB_SZ ?= 4                     # Size of each DB

#EDT Tests
NODE_FANOUT ?= 2               # Fan out for non-leaf EDTs in hierarchical tests
LEAF_FANOUT ?= 100             # Fan out for leaf EDTs in hierarchical tests
TREE_DEPTH ?= 8                # Tree depth for hierarchial tests

DB_NB_ELT ?= 10                # When creating DBs, how many units should each be?
DB_TYPE ?= u64                 # Size of each unit in the above
PARAMC_SIZE ?= 0               # Number of params for each EDT

C_DEFINES := -DDB_NBS=$(DB_NBS) -DNB_ITERS=$(NB_ITERS) -DNB_INSTANCES=$(NB_INSTANCES)\
             -DDRIVER_DEPV=$(DRIVER_DEPV) -DDRIVER_PARAMC=$(DRIVER_PARAMC) -DFAN_OUT=$(FAN_OUT)\
             -DDRIVER_PROP=$(DRIVER_PROP) -DDB_SZ=$(DB_SZ) -DNODE_FANOUT=$(NODE_FANOUT)\
             -DLEAF_FANOUT=$(LEAF_FANOUT) -DTREE_DEPTH=$(TREE_DEPTH) -DDB_NB_ELT=$(DB_NB_ELT)\
             -DDB_TYPE=$(DB_TYPE) -DPARAMC_SIZE=$(PARAMC_SIZE)
