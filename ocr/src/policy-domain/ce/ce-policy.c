/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

// TODO:  The prescription needs to be derived from the affinity, and needs to default to something sensible.
// TODO:  All this prescription logic needs to be moved out of the PD, into an abstract allocator level.  See and resolve TRAC #145.
#define PRESCRIPTION 0xFEDCBA9876544321LL   // L1 remnant, L2 slice, L2 remnant, L3 slice (twice), L3 remnant, remaining levels each slice-then-remnant.
#define PRESCRIPTION_HINT_MASK 0x1
#define PRESCRIPTION_HINT_NUMBITS 1
#define PRESCRIPTION_LEVEL_MASK 0x7
#define PRESCRIPTION_LEVEL_NUMBITS 3
#define NUM_MEM_LEVELS_SUPPORTED 8

#include "ocr-config.h"
#ifdef ENABLE_POLICY_DOMAIN_CE

#include "debug.h"
#include "ocr-errors.h"
#include "ocr-policy-domain.h"
#include "ocr-sysboot.h"

#ifdef OCR_ENABLE_STATISTICS
#include "ocr-statistics.h"
#endif

#include "ocr-comm-platform.h"
#include "policy-domain/ce/ce-policy.h"
#include "allocator/allocator-all.h"

#ifdef HAL_FSIM_CE
#include "rmd-map.h"
#endif

#define DEBUG_TYPE POLICY

extern void ocrShutdown(void);

// Function to cause run-level switches in this PD
u8 cePdSwitchRunlevel(ocrPolicyDomain_t *policy, ocrRunlevel_t runlevel, u32 properties) {
    s32 i=0, j, curPhase, phaseCount;
    u32 maxCount;

    u8 toReturn = 0;
    u32 origProperties = properties;
    u32 propertiesPreComputes = properties;

#define GET_PHASE(counter) curPhase = (properties & RL_BRING_UP)?counter:(phaseCount - counter - 1)

    ocrPolicyDomainCe_t* rself = (ocrPolicyDomainCe_t*)policy;
    // Check properties
    u32 amNodeMaster = properties & RL_NODE_MASTER;
    u32 amPDMaster = properties & RL_PD_MASTER;
    properties &= ~(RL_NODE_MASTER); // Strip out this from the rest; only valuable for the PD and some
                                     // specific workers

    u32 fromPDMsg = properties & RL_FROM_MSG;
    properties &= ~RL_FROM_MSG; // Strip this out from the rest; only valuable for the PD

    // This is important before computes (some modules may do something different for the first thread)
    propertiesPreComputes = properties;
    if(amPDMaster) propertiesPreComputes |= RL_PD_MASTER;

    if(!(fromPDMsg)) {
        // RL changes called directly through switchRunlevel should
        // only transition until PD_OK. After that, transitions should
        // occur using policy messages
        ASSERT(amNodeMaster || (runlevel <= RL_PD_OK));

        // If this is direct function call, it should only be a request
        // if (!((properties & RL_REQUEST) && !(properties & (RL_RESPONSE | RL_RELEASE)))) {
        //     PRINTF("HERE\n");
        //     while(1);
        // }
        ASSERT((properties & RL_REQUEST) && !(properties & (RL_RESPONSE | RL_RELEASE)))
    }
#if 0
    switch(runlevel) {
    case RL_CONFIG_PARSE:
    {
        // Are we bringing the machine up
        if(properties & RL_BRING_UP) {
            for(i = 0; i < RL_MAX; ++i) {
                for(j = 0; j < RL_PHASE_MAX; ++j) {
                    policy->phasesPerRunlevel[i][j] = (1<<4) + 1; // One phase for everything at least
                }
            }

            phaseCount = 2;
        } else {
            // Tear down
            phaseCount = policy->phasesPerRunlevel[RL_CONFIG_PARSE][0] >> 4;
        }
        // Both cases
        maxCount = policy->workerCount;
        for(i = 0; i < phaseCount; ++i) {
            if(toReturn) break;
            GET_PHASE(i);
            toReturn |= helperSwitchInert(policy, runlevel, curPhase, propertiesPreComputes);
            for(j = 0; j < maxCount; ++j) {
                toReturn |= policy->workers[j]->fcts.switchRunlevel(
                    policy->workers[j], policy, runlevel, curPhase, propertiesPreComputes, NULL, 0);
            }
        }
        if(toReturn) {
            DPRINTF(DEBUG_LVL_WARN, "RL_CONFIG_PARSE(%d) phase %d failed: %d\n", propertiesPreComputes, curPhase, toReturn);
        }

        if((!toReturn) && (properties & RL_BRING_UP)) {
            // Coalesce the phasesPerRunLevel by taking the maximum
            for(i = 0; i < RL_MAX; ++i) {
                u32 finalCount = policy->phasesPerRunlevel[i][0];
                for(j = 1; j < RL_PHASE_MAX; ++j) {
                    // Deal with UP phase count
                    u32 newCount = 0;
                    newCount = (policy->phasesPerRunlevel[i][j] & 0xF) > (finalCount & 0xF)?
                        (policy->phasesPerRunlevel[i][j] & 0xF):(finalCount & 0xF);
                    // And now the DOWN phase count
                    newCount |= ((policy->phasesPerRunlevel[i][j] >> 4) > (finalCount >> 4)?
                        (policy->phasesPerRunlevel[i][j] >> 4):(finalCount >> 4)) << 4;
                    finalCount = newCount;
                }
                policy->phasesPerRunlevel[i][0] = finalCount;
            }
        }
        break;
    }
    case RL_NETWORK_OK:
    {
        // In this single PD implementation, nothing specific to do (just pass it down)
        // In general, this is when you setup communication
        phaseCount = ((policy->phasesPerRunlevel[RL_NETWORK_OK][0]) >> ((properties&RL_TEAR_DOWN)?4:0)) & 0xF;
        maxCount = policy->workerCount;
        for(i = 0; i < phaseCount; ++i) {
            if(toReturn) break;
            GET_PHASE(i);
            toReturn |= helperSwitchInert(policy, runlevel, curPhase, propertiesPreComputes);
            for(j = 0; j < maxCount; ++j) {
                toReturn |= policy->workers[j]->fcts.switchRunlevel(
                    policy->workers[j], policy, runlevel, curPhase, propertiesPreComputes, NULL, 0);
            }
        }
        if(toReturn) {
            DPRINTF(DEBUG_LVL_WARN, "RL_NETWORK_OK(%d) phase %d failed: %d\n", propertiesPreComputes, curPhase, toReturn);
        }
        break;
    }
    case RL_PD_OK:
    {
        // In this single PD implementation for x86, there is nothing specific to do
        // In general, you need to:
        //     - if not amNodeMaster, start a worker for this PD
        //     - that worker (or the master one) needs to then transition all inert modules to PD_OK
        phaseCount = ((policy->phasesPerRunlevel[RL_PD_OK][0]) >> ((properties&RL_TEAR_DOWN)?4:0)) & 0xF;

        maxCount = policy->workerCount;
        for(i = 0; i < phaseCount; ++i) {
            if(toReturn) break;
            GET_PHASE(i);
            toReturn |= helperSwitchInert(policy, runlevel, curPhase, propertiesPreComputes);
            for(j = 0; j < maxCount; ++j) {
                toReturn |= policy->workers[j]->fcts.switchRunlevel(
                    policy->workers[j], policy, runlevel, curPhase, propertiesPreComputes, NULL, 0);
            }
        }

        if(toReturn) {
            DPRINTF(DEBUG_LVL_WARN, "RL_PD_OK(%d) phase %d failed: %d\n", propertiesPreComputes, curPhase, toReturn);
        }
        break;
    }
    case RL_MEMORY_OK:
    {
        phaseCount = ((policy->phasesPerRunlevel[RL_MEMORY_OK][0]) >> ((properties&RL_TEAR_DOWN)?4:0)) & 0xF;

        maxCount = policy->workerCount;
        for(i = 0; i < phaseCount; ++i) {
            if(toReturn) break;
            GET_PHASE(i);
            toReturn |= helperSwitchInert(policy, runlevel, curPhase, propertiesPreComputes);
            for(j = 0; j < maxCount; ++j) {
                toReturn |= policy->workers[j]->fcts.switchRunlevel(
                    policy->workers[j], policy, runlevel, curPhase, propertiesPreComputes, NULL, 0);
            }
        }
        if(toReturn) {
            DPRINTF(DEBUG_LVL_WARN, "RL_MEMORY_OK(%d) phase %d failed: %d\n", propertiesPreComputes, curPhase, toReturn);
        }
        break;
    }
    case RL_GUID_OK:
    {
        // In the general case (with more than one PD), in this step and on bring-up:
        //     - send messages to all neighboring PDs to transition to this state
        //     - do local transition
        //     - wait for responses from neighboring PDs
        //     - report back to caller (if RL_FROM_MSG)
        //     - send release message to neighboring PDs
        // If this is RL_FROM_MSG, the above steps may occur in multiple steps (ie: you
        // don't actually "wait" but rather re-enter this function on incomming
        // messages from neighbors. If not RL_FROM_MSG, you do block.

        if(properties & RL_BRING_UP) {
            // On BRING_UP, bring up GUID provider
            // We assert that there are two phases. The first phase is mostly to bring
            // up the GUID provider and the last phase is to actually get GUIDs for
            // the various components if needed
            phaseCount = policy->phasesPerRunlevel[RL_GUID_OK][0] & 0xF;
            maxCount = policy->workerCount;

            for(i = 0; i < phaseCount; ++i) {
                if(toReturn) break;
                toReturn |= helperSwitchInert(policy, runlevel, i, propertiesPreComputes);
                for(j = 0; j < maxCount; ++j) {
                    toReturn |= policy->workers[j]->fcts.switchRunlevel(
                        policy->workers[j], policy, runlevel, i, propertiesPreComputes, NULL, 0);
                }
                if(i == phaseCount - 2) {
                    // I "guidify" myself right before the last phase

                }
            }
        } else {
            // Tear down. We also need a minimum of 2 phases
            // In the first phase, components destroy their GUIDs
            // In the last phase, the GUID provider can go down
            phaseCount = policy->phasesPerRunlevel[RL_GUID_OK][0] >> 4;
            maxCount = policy->workerCount;
            for(i = 0; i < phaseCount; ++i) {
                if(toReturn) break;
                toReturn |= helperSwitchInert(policy, runlevel, i, propertiesPreComputes);
                for(j = 0; j < maxCount; ++j) {
                    toReturn |= policy->workers[j]->fcts.switchRunlevel(
                        policy->workers[j], policy, runlevel, i, propertiesPreComputes, NULL, 0);
                }
                if(i == 0) {
                    // TODO Destroy my GUID
                    // May not be needed but cleaner
                }
            }
        }

        if(toReturn) {
            DPRINTF(DEBUG_LVL_WARN, "RL_GUID_OK(%d) phase %d failed: %d\n", propertiesPreComputes, i-1, toReturn);
        }
        break;
    }

    case RL_COMPUTE_OK:
    {
        // At this stage, we have a memory to use so we can create the placer
        // This phase is the first one creating capable modules (workers) apart from myself
        if(properties & RL_BRING_UP) {
            phaseCount = policy->phasesPerRunlevel[RL_COMPUTE_OK][0] & 0xF;
            maxCount = policy->workerCount;
            for(i = rself->rlSwitch.nextPhase; i < phaseCount; ++i) {
                if(RL_IS_FIRST_PHASE_UP(policy, RL_COMPUTE_OK, i)) {
                    guidify(policy, (u64)policy, &(policy->fguid), OCR_GUID_POLICY);
                    // Create and initialize the placer (work in progress)
                    policy->placer = createLocationPlacer(policy);
                }
                toReturn |= helperSwitchInert(policy, runlevel, i, properties);

                // Setup the resume RL switch structure (in the synchronous case, used as
                // the counter we wait on)
                rself->rlSwitch.checkedIn = maxCount;
                rself->rlSwitch.runlevel = RL_COMPUTE_OK;
                rself->rlSwitch.nextPhase = i + 1;
                rself->rlSwitch.properties = origProperties;
                hal_fence();

                // Worker 0 is considered the capable one by convention
                toReturn |= policy->workers[0]->fcts.switchRunlevel(
                    policy->workers[0], policy, runlevel, i, properties | RL_PD_MASTER | RL_BLESSED,
                    &hcWorkerCallback, RL_COMPUTE_OK << 16);

                for(j = 1; j < maxCount; ++j) {
                    toReturn |= policy->workers[j]->fcts.switchRunlevel(
                        policy->workers[j], policy, runlevel, i, properties, &hcWorkerCallback, (RL_COMPUTE_OK << 16) | j);
                }
                if(!fromPDMsg) {
                    // Here we need to block because when we return from the function, we need to have
                    // transitioned
                    DPRINTF(DEBUG_LVL_VVERB, "switchRunlevel: synchronous switch to RL_COMPUTE_OK phase %d ... will block\n", i);
                    while(rself->rlSwitch.checkedIn) ;
                    ASSERT(rself->rlSwitch.checkedIn == 0);
                } else {
                    DPRINTF(DEBUG_LVL_VVERB, "switchRunlevel: asynchronous switch to RL_COMPUTE_OK phase %d\n", i);
                    // We'll continue this from hcWorkerCallback
                    break; // Break out of the loop
                }
            }
        } else {
            // Tear down
            phaseCount = RL_GET_PHASE_COUNT_DOWN(policy, RL_COMPUTE_OK);
            // On bring down, we need to have at least two phases:
            //     - one where we actually stop the workers (asynchronously)
            //     - one where we join the workers (to clean up properly)
            ASSERT(phaseCount > 1);
            maxCount = policy->workerCount;

            // We do something special for the last phase in which we only have
            // one worker (all others should no longer be operating
            if(RL_IS_LAST_PHASE_DOWN(policy, RL_COMPUTE_OK, rself->rlSwitch.nextPhase)) {
                ASSERT(!fromPDMsg); // This last phase is done synchronously
                ASSERT(amPDMaster); // Only master worker should be here
                toReturn |= helperSwitchInert(policy, runlevel, rself->rlSwitch.nextPhase, properties);
                toReturn |= policy->workers[0]->fcts.switchRunlevel(
                    policy->workers[0], policy, runlevel, rself->rlSwitch.nextPhase,
                    properties | RL_PD_MASTER | RL_BLESSED, NULL, 0);
                for(j = 1; j < maxCount; ++j) {
                    toReturn |= policy->workers[j]->fcts.switchRunlevel(
                        policy->workers[j], policy, runlevel, rself->rlSwitch.nextPhase, properties, NULL, 0);
                }
            } else {
                for(i = rself->rlSwitch.nextPhase; i > 0; --i) {
                    toReturn |= helperSwitchInert(policy, runlevel, i, properties);

                    // Setup the resume RL switch structure (in the synchronous case, used as
                    // the counter we wait on)
                    rself->rlSwitch.checkedIn = maxCount;
                    rself->rlSwitch.runlevel = RL_COMPUTE_OK;
                    rself->rlSwitch.nextPhase = i - 1;
                    rself->rlSwitch.properties = origProperties;
                    hal_fence();

                    // Worker 0 is considered the capable one by convention
                    toReturn |= policy->workers[0]->fcts.switchRunlevel(
                        policy->workers[0], policy, runlevel, i, properties | RL_PD_MASTER | RL_BLESSED,
                        &hcWorkerCallback, RL_COMPUTE_OK << 16);

                    for(j = 1; j < maxCount; ++j) {
                        toReturn |= policy->workers[j]->fcts.switchRunlevel(
                            policy->workers[j], policy, runlevel, i, properties, &hcWorkerCallback, (RL_COMPUTE_OK << 16) | j);
                    }
                    if(!fromPDMsg) {
                        ASSERT(0); // Always from a PD message since it is from a shutdown message
                    } else {
                        DPRINTF(DEBUG_LVL_VVERB, "switchRunlevel: asynchronous switch from RL_COMPUTE_OK phase %d\n", i);
                        // We'll continue this from hcWorkerCallback
                        break;
                    }
                }
            }
        }
        if(toReturn) {
            DPRINTF(DEBUG_LVL_WARN, "RL_COMPUTE_OK(%d) phase %d failed: %d\n", properties, i-1, toReturn);
        }
        break;
    }
    case RL_USER_OK:
    {
        if(properties & RL_BRING_UP) {
            phaseCount = RL_GET_PHASE_COUNT_UP(policy, RL_USER_OK);
            maxCount = policy->workerCount;
            for(i = 0; i < phaseCount - 1; ++i) {
                if(toReturn) break;
                toReturn |= helperSwitchInert(policy, runlevel, i, properties);
                toReturn |= policy->workers[0]->fcts.switchRunlevel(
                    policy->workers[0], policy, runlevel, i, properties | RL_PD_MASTER | RL_BLESSED, NULL, 0);
                for(j = 1; j < maxCount; ++j) {
                    // We start them in an async manner but don't need any callback (ie: we
                    // don't care if they have really started) since there is no bring-up barrier)
                    toReturn |= policy->workers[j]->fcts.switchRunlevel(
                        policy->workers[j], policy, runlevel, i, properties, NULL, 0);
                }
            }
            if(i == phaseCount - 1) { // Tests if we did not break out earlier with if(toReturn)
                toReturn |= helperSwitchInert(policy, runlevel, i, properties);
                for(j = 1; j < maxCount; ++j) {
                    // We start them in an async manner but don't need any callback (ie: we
                    // don't care if they have really started) since there is no bring-up barrier)
                    toReturn |= policy->workers[j]->fcts.switchRunlevel(
                        policy->workers[j], policy, runlevel, i, properties, NULL, 0);
                }
                // Always do the capable worker last in this case (it will actualy start doing something useful)
                toReturn |= policy->workers[0]->fcts.switchRunlevel(
                    policy->workers[0], policy, runlevel, i, properties | RL_PD_MASTER | RL_BLESSED, NULL, 0);
                // When I drop out of this, I should be in RL_COMPUTE_OK at phase 0
                // wait for everyone to check in so that I can continue shutting down
                DPRINTF(DEBUG_LVL_VVERB, "PD_MASTER worker dropped out... waiting for others to complete RL\n");

                while(rself->rlSwitch.checkedIn != 0) ;

                ASSERT(rself->rlSwitch.runlevel == RL_COMPUTE_OK && rself->rlSwitch.nextPhase == 0);
                DPRINTF(DEBUG_LVL_VVERB, "PD_MASTER worker wrapping up shutdown\n");
                // We complete the RL_COMPUTE_OK stage which will bring us down to RL_MEMORY_OK which will
                // get wrapped up by the outside code
                rself->rlSwitch.properties &= ~RL_FROM_MSG;
                toReturn |= policy->fcts.switchRunlevel(policy, rself->rlSwitch.runlevel,
                                                        rself->rlSwitch.properties | RL_PD_MASTER);
            }
        } else {
            // Tear down
            phaseCount = RL_GET_PHASE_COUNT_DOWN(policy, RL_USER_OK);
            maxCount = policy->workerCount;
            for(i = rself->rlSwitch.nextPhase; i >= 0; --i) {
                toReturn |= helperSwitchInert(policy, runlevel, i, properties);

                // Setup the resume RL switch structure (in the synchronous case, used as
                // the counter we wait on)
                rself->rlSwitch.checkedIn = maxCount;
                rself->rlSwitch.runlevel = RL_USER_OK;
                rself->rlSwitch.nextPhase = i - 1;
                rself->rlSwitch.properties = origProperties;
                hal_fence();

                // Worker 0 is considered the capable one by convention
                toReturn |= policy->workers[0]->fcts.switchRunlevel(
                    policy->workers[0], policy, runlevel, i, properties | RL_PD_MASTER | RL_BLESSED,
                    &hcWorkerCallback, RL_USER_OK << 16);

                for(j = 1; j < maxCount; ++j) {
                    toReturn |= policy->workers[j]->fcts.switchRunlevel(
                        policy->workers[j], policy, runlevel, i, properties, &hcWorkerCallback, (RL_USER_OK << 16) | j);
                }
                if(!fromPDMsg) {
                    ASSERT(0); // It should always be from a PD MSG since it is an asynchronous shutdown
                } else {
                    DPRINTF(DEBUG_LVL_VVERB, "switchRunlevel: asynchronous switch from RL_USER_OK phase %d\n", i);
                    // We'll continue this from hcWorkerCallback
                    break;
                }
            }
        }
        if(toReturn) {
            DPRINTF(DEBUG_LVL_WARN, "RL_USER_OK(%d) phase %d failed: %d\n", properties, i-1, toReturn);
        }
        break;
    }
    default:
        // Unknown runlevel
        ASSERT(0);
    }
#endif
    return toReturn;
}

#if 0
void cePolicyDomainBegin(ocrPolicyDomain_t * policy) {
    DPRINTF(DEBUG_LVL_VVERB, "cePolicyDomainBegin called on policy at 0x%lx\n", (u64) policy);
    // The PD should have been brought up by now and everything instantiated

    u64 i = 0;
    u64 maxCount = 0;

    maxCount = policy->commApiCount;
    for(i = 0; i < maxCount; i++) {
        policy->commApis[i]->fcts.begin(policy->commApis[i], policy);
    }

    maxCount = policy->guidProviderCount;
    for(i = 0; i < maxCount; ++i) {
        policy->guidProviders[i]->fcts.begin(policy->guidProviders[i], policy);
    }

    maxCount = policy->allocatorCount;
    for(i = 0; i < maxCount; ++i) {
        policy->allocators[i]->fcts.begin(policy->allocators[i], policy);
    }

    maxCount = policy->schedulerCount;
    for(i = 0; i < maxCount; ++i) {
        policy->schedulers[i]->fcts.begin(policy->schedulers[i], policy);
    }

    policy->placer = NULL;

    // REC: Moved all workers to start here.
    // Note: it's important to first logically start all workers.
    // Once they are all up, start the runtime.
    // Workers should start the underlying target and platforms
    maxCount = policy->workerCount;
    for(i = 0; i < maxCount; i++) {
        policy->workers[i]->fcts.begin(policy->workers[i], policy);
    }

#ifdef HAL_FSIM_CE
    // Neighbor discovery
    // In the general case, all other CEs in my unit are my neighbors
    // However, if I'm block 0 CE, my neighbors also include unit 0 block 0 CE
    // unless I'm unit 0 block 0 CE

    u32 ncount = 0;

    if(policy->neighborCount) {
        for (i = 0; i < MAX_NUM_BLOCK; i++) {
            u32 myUnit = ((policy->myLocation & ID_UNIT_MASK) >> ID_UNIT_SHIFT);
            if(policy->myLocation != MAKE_CORE_ID(0, 0, 0, myUnit, i, ID_AGENT_CE))
                policy->neighbors[ncount++] = MAKE_CORE_ID(0, 0, 0, myUnit, i, ID_AGENT_CE);
            if(ncount >= policy->neighborCount) break;
        }

        // Block 0 of any unit also has block 0 of other units as neighbors
        if((policy->myLocation & ID_BLOCK_MASK) == 0) {
            for (i = 0; i < MAX_NUM_UNIT; i++) {
                if(policy->myLocation != MAKE_CORE_ID(0, 0, 0, i, 0, ID_AGENT_CE))
                    policy->neighbors[ncount++] = MAKE_CORE_ID(0, 0, 0, i, 0, ID_AGENT_CE);
                if(ncount >= policy->neighborCount) break;
            }
        }
    }
    policy->neighborCount = ncount;

#endif

    ocrPolicyDomainCe_t * cePolicy = (ocrPolicyDomainCe_t*)policy;

#ifdef HAL_FSIM_CE
    cePolicy->shutdownMax = cePolicy->xeCount; // All CE's collect their XE's shutdowns
    // Block 0 also collects other blocks in its unit
    if((policy->myLocation & ID_BLOCK_MASK) == 0) {
        cePolicy->shutdownMax += policy->neighborCount;
        // Block 0 of unit 0 collects block 0's of other units
        if(policy->myLocation & ID_UNIT_MASK) {
            u32 otherblocks = 0;
            u32 j;
            for(j = 0; j<policy->neighborCount; j++)
                if((policy->neighbors[j] & ID_BLOCK_MASK) == 0)
                    otherblocks++;
            cePolicy->shutdownMax -= otherblocks;
            // Subtract those neighbors which are not block 0, unit 0
        }
    }
#else
    //FIXME: This is a hack to get the shutdown reportees.
    //This should be replaced by a registration protocol for children to parents.
    //Trac bug: #134
    if (policy->myLocation == policy->parentLocation) {
        cePolicy->shutdownMax = cePolicy->xeCount + policy->neighborCount;
    } else {
        cePolicy->shutdownMax = cePolicy->xeCount;
    }
#endif
}

void cePolicyDomainStart(ocrPolicyDomain_t * policy) {
    DPRINTF(DEBUG_LVL_VVERB, "cePolicyDomainStart called on policy at 0x%lx\n", (u64) policy);
    // The PD should have been brought up by now and everything instantiated
    // This is a bit ugly but I can't find a cleaner solution:
    //   - we need to associate the environment with the
    //   currently running worker/PD so that we can use getCurrentEnv

    ocrPolicyDomainCe_t* cePolicy = (ocrPolicyDomainCe_t*) policy;
    u64 i = 0;
    u64 maxCount = 0;
    u64 low, high, level, engineIdx;

    maxCount = policy->allocatorCount;
    low = 0;
    for(i = 0; i < maxCount; ++i) {
        level = policy->allocators[low]->memories[0]->level;
        high = i+1;
        if (high == maxCount || policy->allocators[high]->memories[0]->level != level) {
            // One or more allocators for some level were provided in the config file.  Their indices in
            // the array of allocators span from low(inclusive) to high (exclusive).  From this information,
            // initialize the allocatorIndexLookup table for that level, for all agents.
            if (high - low == 1) {
                // All agents in the block use the same allocator (usage conjecture: on TG,
                // this is used for all but L1.)
                for (engineIdx = 0; engineIdx <= cePolicy->xeCount; engineIdx++) {
                    policy->allocatorIndexLookup[engineIdx*NUM_MEM_LEVELS_SUPPORTED+level] = low;
                }
            } else if (high - low == 2) {
                // All agents except the last in the block (i.e. the XEs) use the same allocator.
                // The last (i.e. the CE) uses a different allocator.  (usage conjecture: on TG,
                // this is might be used to experiment with XEs sharing a common SPAD that is
                // different than what the CE uses.  This is presently just for academic interst.)
                for (engineIdx = 0; engineIdx < cePolicy->xeCount; engineIdx++) {
                    policy->allocatorIndexLookup[engineIdx*NUM_MEM_LEVELS_SUPPORTED+level] = low;
                }
                policy->allocatorIndexLookup[engineIdx*NUM_MEM_LEVELS_SUPPORTED+level] = low+1;
            } else if (high - low == cePolicy->xeCount+1) {
                // All agents in the block use different allocators (usage conjecture: on TG,
                // this is used for L1.)
                for (engineIdx = 0; engineIdx <= cePolicy->xeCount; engineIdx++) {
                    policy->allocatorIndexLookup[engineIdx*NUM_MEM_LEVELS_SUPPORTED+level] = low+engineIdx;
                }
            } else {
                DPRINTF(DEBUG_LVL_WARN, "I don't know how to spread allocator[%ld:%ld] (level %ld) across %ld XEs plus the CE\n", (u64) low, (u64) high-1, (u64) level, (u64) cePolicy->xeCount);
                ASSERT (0);
            }
            low = high;
        }
    }

    maxCount = policy->guidProviderCount;
    for(i = 0; i < maxCount; ++i) {
        policy->guidProviders[i]->fcts.start(policy->guidProviders[i], policy);
    }

    maxCount = policy->allocatorCount;
    for(i = 0; i < maxCount; ++i) {
        policy->allocators[i]->fcts.start(policy->allocators[i], policy);
    }

    guidify(policy, (u64)policy, &(policy->fguid), OCR_GUID_POLICY);

    maxCount = policy->schedulerCount;
    for(i = 0; i < maxCount; ++i) {
        policy->schedulers[i]->fcts.start(policy->schedulers[i], policy);
    }

    maxCount = policy->commApiCount;
    for(i = 0; i < maxCount; i++) {
        policy->commApis[i]->fcts.start(policy->commApis[i], policy);
    }

    // REC: Moved all workers to start here.
    // Note: it's important to first logically start all workers.
    // Once they are all up, start the runtime.
    // Workers should start the underlying target and platforms
    maxCount = policy->workerCount;
    for(i = 0; i < maxCount; i++) {
        policy->workers[i]->fcts.start(policy->workers[i], policy);
    }
}

void cePolicyDomainStop(ocrPolicyDomain_t * policy, ocrRunLevel_t rl, u32 action) {
    switch(rl) {
        case RL_STOP: {
            // Finish everything in reverse order
            // In CE, we MUST call stop on the master worker first.
            // The master worker enters its work routine loop and will
            // be unlocked by ocrShutdown
            u64 i = 0;
            u64 maxCount = 0;

            // Note: As soon as worker '0' is stopped; its thread is
            // free to fall-through and continue shutting down the
            // policy domain
            maxCount = policy->workerCount;
            for(i = 0; i < maxCount; i++) {
                policy->workers[i]->fcts.stop(policy->workers[i], rl, action);
            }
            // WARNING: Do not add code here unless you know what you're doing !!
            // If we are here, it means an EDT called ocrShutdown which
            // logically finished workers and can make thread '0' executes this
            // code before joining the other threads.

            // Thread '0' joins the other (N-1) threads.

            maxCount = policy->commApiCount;
            for(i = 0; i < maxCount; i++) {
                policy->commApis[i]->fcts.stop(policy->commApis[i], rl, action);
            }

            maxCount = policy->schedulerCount;
            for(i = 0; i < maxCount; ++i) {
                policy->schedulers[i]->fcts.stop(policy->schedulers[i], rl, action);
            }

            maxCount = policy->allocatorCount;
            for(i = 0; i < maxCount; ++i) {
                policy->allocators[i]->fcts.stop(policy->allocators[i], rl, action);
            }

            // We could release our GUID here but not really required

            maxCount = policy->guidProviderCount;
            for(i = 0; i < maxCount; ++i) {
                policy->guidProviders[i]->fcts.stop(policy->guidProviders[i], rl, action);
            }
            break;
        }
        case RL_SHUTDOWN: {
            // Finish everything in reverse order
            u64 i = 0;
            u64 maxCount = 0;

            // Note: As soon as worker '0' is stopped; its thread is
            // free to fall-through and continue shutting down the
            // policy domain
            maxCount = policy->workerCount;
            for(i = 0; i < maxCount; i++) {
                policy->workers[i]->fcts.stop(policy->workers[i], RL_SHUTDOWN, action);
            }

            maxCount = policy->commApiCount;
            for(i = 0; i < maxCount; i++) {
                policy->commApis[i]->fcts.stop(policy->commApis[i], RL_SHUTDOWN, action);
            }

            maxCount = policy->schedulerCount;
            for(i = 0; i < maxCount; ++i) {
                policy->schedulers[i]->fcts.stop(policy->schedulers[i], RL_SHUTDOWN, action);
            }

            maxCount = policy->allocatorCount;
            for(i = 0; i < maxCount; ++i) {
                policy->allocators[i]->fcts.stop(policy->allocators[i], RL_SHUTDOWN, action);
            }

            maxCount = policy->guidProviderCount;
            for(i = 0; i < maxCount; ++i) {
                policy->guidProviders[i]->fcts.stop(policy->guidProviders[i], RL_SHUTDOWN, action);
            }
            break;
        }
        default:
            ASSERT("Unknown runlevel in stop function");
    }
}
#endif
void cePolicyDomainDestruct(ocrPolicyDomain_t * policy) {
    // Destroying instances
    u64 i = 0;
    u64 maxCount = 0;

    // Note: As soon as worker '0' is stopped; its thread is
    // free to fall-through and continue shutting down the
    // policy domain
    maxCount = policy->workerCount;
    for(i = 0; i < maxCount; i++) {
        policy->workers[i]->fcts.destruct(policy->workers[i]);
    }

    maxCount = policy->commApiCount;
    for(i = 0; i < maxCount; i++) {
        policy->commApis[i]->fcts.destruct(policy->commApis[i]);
    }

    maxCount = policy->schedulerCount;
    for(i = 0; i < maxCount; ++i) {
        policy->schedulers[i]->fcts.destruct(policy->schedulers[i]);
    }

    maxCount = policy->allocatorCount;
    for(i = 0; i < maxCount; ++i) {
        policy->allocators[i]->fcts.destruct(policy->allocators[i]);
    }

    // Destruct factories
    maxCount = policy->taskFactoryCount;
    for(i = 0; i < maxCount; ++i) {
        policy->taskFactories[i]->destruct(policy->taskFactories[i]);
    }

    maxCount = policy->taskTemplateFactoryCount;
    for(i = 0; i < maxCount; ++i) {
        policy->taskTemplateFactories[i]->destruct(policy->taskTemplateFactories[i]);
    }

    maxCount = policy->dbFactoryCount;
    for(i = 0; i < maxCount; ++i) {
        policy->dbFactories[i]->destruct(policy->dbFactories[i]);
    }

    maxCount = policy->eventFactoryCount;
    for(i = 0; i < maxCount; ++i) {
        policy->eventFactories[i]->destruct(policy->eventFactories[i]);
    }

    //Anticipate those to be null-impl for some time
    ASSERT(policy->costFunction == NULL);

    // Destroy these last in case some of the other destructs make use of them
    maxCount = policy->guidProviderCount;
    for(i = 0; i < maxCount; ++i) {
        policy->guidProviders[i]->fcts.destruct(policy->guidProviders[i]);
    }

    // Destroy self
    runtimeChunkFree((u64)policy->workers, NULL);
    runtimeChunkFree((u64)policy->schedulers, NULL);
    runtimeChunkFree((u64)policy->allocators, NULL);
    runtimeChunkFree((u64)policy->allocatorIndexLookup, NULL);
    runtimeChunkFree((u64)policy->taskFactories, NULL);
    runtimeChunkFree((u64)policy->taskTemplateFactories, NULL);
    runtimeChunkFree((u64)policy->dbFactories, NULL);
    runtimeChunkFree((u64)policy->eventFactories, NULL);
    runtimeChunkFree((u64)policy->guidProviders, NULL);
    runtimeChunkFree((u64)policy, NULL);
}

static void localDeguidify(ocrPolicyDomain_t *self, ocrFatGuid_t *guid) {
    ASSERT(self->guidProviderCount == 1);
    if(guid->guid != NULL_GUID && guid->guid != UNINITIALIZED_GUID) {
        if(guid->metaDataPtr == NULL) {
            self->guidProviders[0]->fcts.getVal(self->guidProviders[0], guid->guid,
                                                (u64*)(&(guid->metaDataPtr)), NULL);
        }
    }
}

static u8 getEngineIndex(ocrPolicyDomain_t *self, ocrLocation_t location) {

    ocrPolicyDomainCe_t* derived = (ocrPolicyDomainCe_t*) self;

    // We need the block-relative index of the engine (aka agent).  That index for XE's ranges from
    // 0 to xeCount-1, and the engine index for the CE is next, i.e. it equals xeCount.  We derive
    // the index from the system-wide absolute location of the agent.  If a time comes when different
    // chips in a system might have different numbers of XEs per block, this code might break, and
    // have to be reworked.

    ocrLocation_t blockRelativeEngineIndex =
        location -         // Absolute location of the agent for which the allocation is being done.
        self->myLocation + // Absolute location of the CE doing the allocation.
        derived->xeCount;  // Offset so that first XE is 0, last is xeCount-1, and CE is xeCount.
#if defined(HAL_FSIM_CE)
    // FIXME: Temporary override to avoid a bug. Trac #270
    if(location == self->myLocation) blockRelativeEngineIndex = derived->xeCount; else blockRelativeEngineIndex = location & 0xF;
#endif
    ASSERT(blockRelativeEngineIndex >= 0);
    ASSERT(blockRelativeEngineIndex <= derived->xeCount);
    return blockRelativeEngineIndex;
}


// In all these functions, we consider only a single PD. In other words, in CE, we
// deal with everything locally and never send messages

// allocateDatablock:  Utility used by ceAllocateDb and ceMemAlloc, just below.
static void* allocateDatablock (ocrPolicyDomain_t *self,
                                u64                size,
                                u64                engineIndex,
                                u64                prescription,
                                u64               *allocatorIndexReturn) {
    void* result;
    u64 allocatorHints;  // Allocator hint
    u64 levelIndex; // "Level" of the memory hierarchy, taken from the "prescription" to pick from the allocators array the allocator to try.
    s8 allocatorIndex;

    ASSERT (self->allocatorCount > 0);

    do {
        allocatorHints = (prescription & PRESCRIPTION_HINT_MASK) ?
            (OCR_ALLOC_HINT_NONE) :              // If the hint is non-zero, pick this (e.g. for tlsf, tries "remnant" pool.)
            (OCR_ALLOC_HINT_REDUCE_CONTENTION);  // If the hint is zero, pick this (e.g. for tlsf, tries a round-robin-chosen "slice" pool.)
        prescription >>= PRESCRIPTION_HINT_NUMBITS;
        levelIndex = prescription & PRESCRIPTION_LEVEL_MASK;
        prescription >>= PRESCRIPTION_LEVEL_NUMBITS;
        allocatorIndex = self->allocatorIndexLookup[engineIndex*NUM_MEM_LEVELS_SUPPORTED+levelIndex]; // Lookup index of allocator to use for requesting engine (aka agent) at prescribed memory hierarchy level.
        if ((allocatorIndex < 0) ||
            (allocatorIndex >= self->allocatorCount) ||
            (self->allocators[allocatorIndex] == NULL)) continue;  // Skip this allocator if it doesn't exist.
        result = self->allocators[allocatorIndex]->fcts.allocate(self->allocators[allocatorIndex], size, allocatorHints);
        if (result) {
            DPRINTF (DEBUG_LVL_VVERB, "Success allocationg %5ld-byte block to allocator %ld (level %ld) for engine %ld (%2ld) -- 0x%lx\n",
            (u64) size, (u64) allocatorIndex, (u64) levelIndex, (u64) engineIndex, (u64) self->myLocation, (u64) (((u64*) result)[-1]));
            *allocatorIndexReturn = allocatorIndex;
            return result;
        }
    } while (prescription != 0);

    return NULL;
}

static u8 ceAllocateDb(ocrPolicyDomain_t *self, ocrFatGuid_t *guid, void** ptr, u64 size,
                       u32 properties, u64 engineIndex,
                       ocrFatGuid_t affinity, ocrInDbAllocator_t allocator,
                       u64 prescription) {
    // This function allocates a data block for the requestor, who is either this computing agent or a
    // different one that sent us a message.  After getting that data block, it "guidifies" the results
    // which, by the way, ultimately causes ceMemAlloc (just below) to run.
    //
    // Currently, the "affinity" and "allocator" arguments are ignored, and I expect that these will
    // eventually be eliminated here and instead, above this level, processed into the "prescription"
    // variable, which has been added to this argument list.  The prescription indicates an order in
    // which to attempt to allocate the block to a pool.
    u64 idx;
    void* result = allocateDatablock (self, size, engineIndex, prescription, &idx);

    if (result) {
        ocrDataBlock_t *block = self->dbFactories[0]->instantiate(
            self->dbFactories[0], self->allocators[idx]->fguid, self->fguid,
            size, result, properties, NULL);
        *ptr = result;
        (*guid).guid = block->guid;
        (*guid).metaDataPtr = block;
        return 0;
    } else {
        DPRINTF(DEBUG_LVL_WARN, "ceAllocateDb returning NULL for size %ld\n", (u64) size);
        return OCR_ENOMEM;
    }
}

static u8 ceMemAlloc(ocrPolicyDomain_t *self, ocrFatGuid_t* allocator, u64 size,
                     u64 engineIndex, ocrMemType_t memType, void** ptr,
                     u64 prescription) {
    // Like hcAllocateDb, this function also allocates a data block.  But it does NOT guidify
    // the results.  The main usage of this function is to allocate space for the guid needed
    // by ceAllocateDb; so if this function also guidified its results, you'd be in an infinite
    // guidification loop!
    void* result;
    u64 idx;
    ASSERT (memType == GUID_MEMTYPE || memType == DB_MEMTYPE);
    result = allocateDatablock (self, size, engineIndex, prescription, &idx);

    if (result) {
        *ptr = result;
        *allocator = self->allocators[idx]->fguid;
        return 0;
    } else {
        DPRINTF(DEBUG_LVL_WARN, "ceMemAlloc returning NULL for size %ld\n", (u64) size);
        return OCR_ENOMEM;
    }
}

static u8 ceMemUnalloc(ocrPolicyDomain_t *self, ocrFatGuid_t* allocator,
                       void* ptr, ocrMemType_t memType) {
    allocatorFreeFunction(ptr);
    return 0;
}

static u8 ceCreateEdt(ocrPolicyDomain_t *self, ocrFatGuid_t *guid,
                      ocrFatGuid_t  edtTemplate, u32 *paramc, u64* paramv,
                      u32 *depc, u32 properties, ocrFatGuid_t affinity,
                      ocrFatGuid_t * outputEvent, ocrTask_t * currentEdt,
                      ocrFatGuid_t parentLatch) {


    ocrTaskTemplate_t *taskTemplate = (ocrTaskTemplate_t*)edtTemplate.metaDataPtr;

    ASSERT(((taskTemplate->paramc == EDT_PARAM_UNK) && *paramc != EDT_PARAM_DEF) ||
           (taskTemplate->paramc != EDT_PARAM_UNK && (*paramc == EDT_PARAM_DEF ||
                                                      taskTemplate->paramc == *paramc)));
    ASSERT(((taskTemplate->depc == EDT_PARAM_UNK) && *depc != EDT_PARAM_DEF) ||
           (taskTemplate->depc != EDT_PARAM_UNK && (*depc == EDT_PARAM_DEF ||
                                                    taskTemplate->depc == *depc)));

    if(*paramc == EDT_PARAM_DEF) {
        *paramc = taskTemplate->paramc;
    }

    if(*depc == EDT_PARAM_DEF) {
        *depc = taskTemplate->depc;
    }
    // If paramc are expected, double check paramv is not NULL
    if((*paramc > 0) && (paramv == NULL)) {
        ASSERT(0);
        return OCR_EINVAL;
    }

    u8 res = self->taskFactories[0]->instantiate(
        self->taskFactories[0], guid, edtTemplate, *paramc, paramv,
        *depc, properties, affinity, outputEvent, currentEdt, parentLatch, NULL);

    ASSERT(!res);

    return 0;
}

static u8 ceCreateEdtTemplate(ocrPolicyDomain_t *self, ocrFatGuid_t *guid,
                              ocrEdt_t func, u32 paramc, u32 depc, const char* funcName) {


    ocrTaskTemplate_t *base = self->taskTemplateFactories[0]->instantiate(
        self->taskTemplateFactories[0], func, paramc, depc, funcName, NULL);
    (*guid).guid = base->guid;
    (*guid).metaDataPtr = base;
    return 0;
}

static u8 ceCreateEvent(ocrPolicyDomain_t *self, ocrFatGuid_t *guid,
                        ocrEventTypes_t type, bool takesArg) {

    ocrEvent_t *base = self->eventFactories[0]->instantiate(
        self->eventFactories[0], type, takesArg, NULL);
    (*guid).guid = base->guid;
    (*guid).metaDataPtr = base;
    return 0;
}

static u8 convertDepAddToSatisfy(ocrPolicyDomain_t *self, ocrFatGuid_t dbGuid,
                                   ocrFatGuid_t destGuid, u32 slot) {

    PD_MSG_STACK(msg);
    ocrTask_t *curTask = NULL;
    getCurrentEnv(NULL, NULL, &curTask, &msg);
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_DEP_SATISFY
    msg.type = PD_MSG_DEP_SATISFY | PD_MSG_REQUEST;
    PD_MSG_FIELD_I(satisfierGuid.guid) = curTask?curTask->guid:NULL_GUID;
    PD_MSG_FIELD_I(satisfierGuid.metaDataPtr) = curTask;
    PD_MSG_FIELD_I(guid) = destGuid;
    PD_MSG_FIELD_I(payload) = dbGuid;
    PD_MSG_FIELD_I(slot) = slot;
    PD_MSG_FIELD_I(properties) = 0;
    RESULT_PROPAGATE(self->fcts.processMessage(self, &msg, false));
#undef PD_MSG
#undef PD_TYPE
    return 0;
}

#ifdef OCR_ENABLE_STATISTICS
static ocrStats_t* ceGetStats(ocrPolicyDomain_t *self) {
    return self->statsObject;
}
#endif

static u8 ceProcessResponse(ocrPolicyDomain_t *self, ocrPolicyMsg_t *msg, u32 properties) {
    if (msg->srcLocation == self->myLocation) {
        msg->type &= ~PD_MSG_REQUEST;
        msg->type |=  PD_MSG_RESPONSE;
    } else {
        msg->destLocation = msg->srcLocation;
        msg->srcLocation = self->myLocation;
        if((msg->type & PD_CE_CE_MESSAGE) && (msg->type & PD_MSG_REQUEST)) {
             // FIXME: Ugly, but no other way to destruct the message
             self->commApis[0]->commPlatform->fcts.destructMessage(self->commApis[0]->commPlatform, msg);
        }
#if 0
        if((((ocrPolicyDomainCe_t*)self)->shutdownMode) &&
           (msg->type != PD_MSG_MGT_SHUTDOWN)) {
            msg->type &= ~PD_MSG_TYPE_ONLY;
            msg->type |= PD_MSG_MGT_SHUTDOWN;
            properties |= BLOCKING_SEND_MSG_PROP;
        }
#endif
        if (msg->type & PD_MSG_REQ_RESPONSE) {
            msg->type &= ~PD_MSG_REQUEST;
            msg->type |=  PD_MSG_RESPONSE;
#if defined(HAL_FSIM_CE)
            if((msg->destLocation & ID_AGENT_CE) == ID_AGENT_CE) {
                // This is a CE->CE message
                PD_MSG_STACK(toSend);
                u64 baseSize = 0, marshalledSize = 0;
                ocrPolicyMsgGetMsgSize(msg, &baseSize, &marshalledSize, 0);
                ocrPolicyMsgMarshallMsg(msg, baseSize, (u8 *)&toSend, MARSHALL_DUPLICATE);
                while(self->fcts.sendMessage(self, toSend.destLocation, &toSend, NULL, properties) == 1) {
                    PD_MSG_STACK(msg);
                    ocrMsgHandle_t myHandle;
                    myHandle.msg = &msg;
                    ocrMsgHandle_t *handle = &myHandle;
                    while(!self->fcts.pollMessage(self, &handle))
                        RESULT_ASSERT(self->fcts.processMessage(self, handle->response, true), ==, 0);
                }
            } else {
/*                // This is a CE->XE message
                if((msg->type & PD_MSG_TYPE_ONLY) != PD_MSG_MGT_SHUTDOWN) {
                    RESULT_ASSERT(self->fcts.sendMessage(self, msg->destLocation, msg, NULL, properties), ==, 0);
                } else  // Temporary workaround till bug #134 is fixed
                    self->fcts.sendMessage(self, msg->destLocation, msg, NULL, properties);
*/
            }
#else
            RESULT_ASSERT(self->fcts.sendMessage(self, msg->destLocation, msg, NULL, properties), ==, 0);
#endif
        }
    }
    return 0;
}

u8 cePolicyDomainProcessMessage(ocrPolicyDomain_t *self, ocrPolicyMsg_t *msg, u8 isBlocking) {

    u8 returnCode = 0;

    DPRINTF(DEBUG_LVL_VERB, "Going to processs message of type 0x%x\n",
            msg->type & PD_MSG_TYPE_ONLY);

    switch((u32)(msg->type & PD_MSG_TYPE_ONLY)) {
    // Some messages are not handled at all
    case PD_MSG_DB_DESTROY:
    case PD_MSG_WORK_EXECUTE: case PD_MSG_DEP_UNREGSIGNALER:
    case PD_MSG_DEP_UNREGWAITER: case PD_MSG_DEP_DYNADD:
    case PD_MSG_DEP_DYNREMOVE:
    case PD_MSG_GUID_METADATA_CLONE:
    case PD_MSG_SAL_TERMINATE: case PD_MSG_MGT_REGISTER:
    case PD_MSG_MGT_UNREGISTER:
    case PD_MSG_MGT_MONITOR_PROGRESS:
    {
        DPRINTF(DEBUG_LVL_WARN, "CE PD does not handle call of type 0x%x\n",
                (u32)(msg->type & PD_MSG_TYPE_ONLY));
        ASSERT(0);
    }

    // Most messages are handled locally
    case PD_MSG_DB_CREATE: {
        START_PROFILE(pd_ce_DbCreate);
#define PD_MSG msg
#define PD_TYPE PD_MSG_DB_CREATE
        // TODO: Add properties whether DB needs to be acquired or not
        // This would impact where we do the PD_MSG_MEM_ALLOC for example
        // For now we deal with both USER and RT dbs the same way
        ASSERT((PD_MSG_FIELD_I(dbType) == USER_DBTYPE) || (PD_MSG_FIELD_I(dbType) == RUNTIME_DBTYPE));
        DPRINTF(DEBUG_LVL_VVERB, "DB_CREATE request from 0x%lx for size %lu\n",
                msg->srcLocation, PD_MSG_FIELD_IO(size));
// TODO:  The prescription needs to be derived from the affinity, and needs to default to something sensible.
        u64 engineIndex = getEngineIndex(self, msg->srcLocation);
        ocrFatGuid_t edtFatGuid = {.guid = PD_MSG_FIELD_I(edt.guid), .metaDataPtr = PD_MSG_FIELD_I(edt.metaDataPtr)};
        u64 reqSize = PD_MSG_FIELD_IO(size);

        PD_MSG_FIELD_O(returnDetail) = ceAllocateDb(
            self, &(PD_MSG_FIELD_IO(guid)), &(PD_MSG_FIELD_O(ptr)), reqSize,
            PD_MSG_FIELD_IO(properties), engineIndex,
            PD_MSG_FIELD_I(affinity), PD_MSG_FIELD_I(allocator), PRESCRIPTION);
        if(PD_MSG_FIELD_O(returnDetail) == 0) {
            ocrDataBlock_t *db= PD_MSG_FIELD_IO(guid.metaDataPtr);
            ASSERT(db);
            // TODO: Check if properties want DB acquired
            ASSERT(db->fctId == self->dbFactories[0]->factoryId);
            PD_MSG_FIELD_O(returnDetail) = self->dbFactories[0]->fcts.acquire(
                db, &(PD_MSG_FIELD_O(ptr)), edtFatGuid, EDT_SLOT_NONE,
                DB_MODE_ITW, PD_MSG_FIELD_IO(properties) & DB_PROP_RT_ACQUIRE, (u32)DB_MODE_ITW);
        } else {
            // Cannot acquire
            PD_MSG_FIELD_O(ptr) = NULL;
        }
        DPRINTF(DEBUG_LVL_VVERB, "DB_CREATE response for size %lu: GUID: 0x%lx; PTR: 0x%lx)\n",
                reqSize, PD_MSG_FIELD_IO(guid.guid), PD_MSG_FIELD_O(ptr));
        returnCode = ceProcessResponse(self, msg, 0);
#undef PD_MSG
#undef PD_TYPE
        EXIT_PROFILE;
        break;
    }

    case PD_MSG_DB_ACQUIRE: {
        START_PROFILE(pd_ce_DbAcquire);
        // Call the appropriate acquire function
#define PD_MSG msg
#define PD_TYPE PD_MSG_DB_ACQUIRE
        if (msg->type & PD_MSG_REQUEST) {
            localDeguidify(self, &(PD_MSG_FIELD_IO(guid)));
            localDeguidify(self, &(PD_MSG_FIELD_IO(edt)));
            ocrDataBlock_t *db = (ocrDataBlock_t*)(PD_MSG_FIELD_IO(guid.metaDataPtr));
            DPRINTF(DEBUG_LVL_VVERB, "DB_ACQUIRE request from 0x%lx for GUID 0x%lx\n",
                    msg->srcLocation, PD_MSG_FIELD_IO(guid.guid));
            ASSERT(db->fctId == self->dbFactories[0]->factoryId);
            //ASSERT(!(msg->type & PD_MSG_REQ_RESPONSE));
            PD_MSG_FIELD_O(returnDetail) = self->dbFactories[0]->fcts.acquire(
                db, &(PD_MSG_FIELD_O(ptr)), PD_MSG_FIELD_IO(edt), PD_MSG_FIELD_IO(edtSlot),
                (ocrDbAccessMode_t)(PD_MSG_FIELD_IO(properties) & (u32)DB_ACCESS_MODE_MASK),
                PD_MSG_FIELD_IO(properties) & DB_PROP_RT_ACQUIRE, PD_MSG_FIELD_IO(properties));
            PD_MSG_FIELD_O(size) = db->size;
            PD_MSG_FIELD_IO(properties) |= db->flags;
            // NOTE: at this point the DB may not have been acquired.
            // In that case PD_MSG_FIELD_O(returnDetail) == OCR_EBUSY
            // The XE checks for this return code.
            DPRINTF(DEBUG_LVL_VVERB, "DB_ACQUIRE response for GUID 0x%lx: PTR: 0x%lx\n",
                    PD_MSG_FIELD_IO(guid.guid), PD_MSG_FIELD_O(ptr));
            returnCode = ceProcessResponse(self, msg, 0);
        } else {
            // This a callback response to an acquire that was pending. The CE just need
            // to call the task's dependenceResolve to iterate the task's acquire frontier
            ASSERT(msg->type & PD_MSG_RESPONSE);
            // asynchronous callback on acquire, reading response
            ocrFatGuid_t edtFGuid = PD_MSG_FIELD_IO(edt);
            ocrFatGuid_t dbFGuid = PD_MSG_FIELD_IO(guid);
            u32 edtSlot = PD_MSG_FIELD_IO(edtSlot);
            localDeguidify(self, &edtFGuid);
            // At this point the edt MUST be local as well as the db data pointer.
            ocrTask_t* task = (ocrTask_t*) edtFGuid.metaDataPtr;
            PD_MSG_FIELD_O(returnDetail) = self->taskFactories[0]->fcts.dependenceResolved(task, dbFGuid.guid, PD_MSG_FIELD_O(ptr), edtSlot);
        }
#undef PD_MSG
#undef PD_TYPE
        EXIT_PROFILE;
        break;
    }

    case PD_MSG_DB_RELEASE: {
        START_PROFILE(pd_ce_DbRelease);
        // Call the appropriate release function
#define PD_MSG msg
#define PD_TYPE PD_MSG_DB_RELEASE
        localDeguidify(self, &(PD_MSG_FIELD_IO(guid)));
        localDeguidify(self, &(PD_MSG_FIELD_I(edt)));
        ocrDataBlock_t *db = (ocrDataBlock_t*)(PD_MSG_FIELD_IO(guid.metaDataPtr));
        ASSERT(db->fctId == self->dbFactories[0]->factoryId);
        //ASSERT(!(msg->type & PD_MSG_REQ_RESPONSE));
        DPRINTF(DEBUG_LVL_VVERB, "DB_RELEASE req/resp from 0x%lx for GUID 0x%lx\n",
                msg->srcLocation, PD_MSG_FIELD_IO(guid.guid));
        PD_MSG_FIELD_O(returnDetail) =
            self->dbFactories[0]->fcts.release(db, PD_MSG_FIELD_I(edt), PD_MSG_FIELD_I(properties) & DB_PROP_RT_ACQUIRE);
        returnCode = ceProcessResponse(self, msg, 0);
#undef PD_MSG
#undef PD_TYPE
        EXIT_PROFILE;
        break;
    }

    case PD_MSG_DB_FREE: {
        START_PROFILE(pd_ce_DbFree);
        // Call the appropriate free function
#define PD_MSG msg
#define PD_TYPE PD_MSG_DB_FREE
        localDeguidify(self, &(PD_MSG_FIELD_I(guid)));
        localDeguidify(self, &(PD_MSG_FIELD_I(edt)));
        ocrDataBlock_t *db = (ocrDataBlock_t*)(PD_MSG_FIELD_I(guid.metaDataPtr));
        DPRINTF(DEBUG_LVL_VVERB, "DB_FREE req/resp from 0x%lx for GUID 0x%lx\n",
                msg->srcLocation, PD_MSG_FIELD_I(guid.guid));
        ASSERT(db->fctId == self->dbFactories[0]->factoryId);
        ASSERT(!(msg->type & PD_MSG_REQ_RESPONSE));
        PD_MSG_FIELD_O(returnDetail) =
            self->dbFactories[0]->fcts.free(db, PD_MSG_FIELD_I(edt), PD_MSG_FIELD_I(properties) & DB_PROP_RT_ACQUIRE);
        returnCode = ceProcessResponse(self, msg, 0);
#undef PD_MSG
#undef PD_TYPE
        EXIT_PROFILE;
        break;
    }

    case PD_MSG_MEM_ALLOC: {
        START_PROFILE(pd_ce_MemAlloc);
        u64 engineIndex = getEngineIndex(self, msg->srcLocation);
#define PD_MSG msg
#define PD_TYPE PD_MSG_MEM_ALLOC
        DPRINTF(DEBUG_LVL_VVERB, "MEM_ALLOC request from 0x%lx for size %lu\n",
                msg->srcLocation, PD_MSG_FIELD_I(size));
        PD_MSG_FIELD_O(returnDetail) = ceMemAlloc(
            self, &(PD_MSG_FIELD_O(allocator)), PD_MSG_FIELD_I(size),
            engineIndex, PD_MSG_FIELD_I(type), &(PD_MSG_FIELD_O(ptr)),
            PRESCRIPTION);
        PD_MSG_FIELD_O(allocatingPD) = self->fguid;
        DPRINTF(DEBUG_LVL_VVERB, "MEM_ALLOC response: PTR: 0x%lx\n",
                PD_MSG_FIELD_O(ptr));
        returnCode = ceProcessResponse(self, msg, 0);
#undef PD_MSG
#undef PD_TYPE
        EXIT_PROFILE;
        break;
    }

    case PD_MSG_MEM_UNALLOC: {
        START_PROFILE(pd_ce_MemUnalloc);
#define PD_MSG msg
#define PD_TYPE PD_MSG_MEM_UNALLOC
        // REC: Do we really care about the allocator returned and what not?
        ocrFatGuid_t allocGuid = {.guid = PD_MSG_FIELD_I(allocator.guid), .metaDataPtr = PD_MSG_FIELD_I(allocator.metaDataPtr) };
        DPRINTF(DEBUG_LVL_VVERB, "MEM_UNALLOC req/resp from 0x%lx for PTR 0x%lx\n",
                msg->srcLocation, PD_MSG_FIELD_I(ptr));
        PD_MSG_FIELD_O(returnDetail) = ceMemUnalloc(
            self, &allocGuid, PD_MSG_FIELD_I(ptr), PD_MSG_FIELD_I(type));
        returnCode = ceProcessResponse(self, msg, 0);
#undef PD_MSG
#undef PD_TYPE
        EXIT_PROFILE;
        break;
    }

    case PD_MSG_WORK_CREATE: {
        START_PROFILE(pd_ce_WorkCreate);
#define PD_MSG msg
#define PD_TYPE PD_MSG_WORK_CREATE
        localDeguidify(self, &(PD_MSG_FIELD_I(templateGuid)));
        localDeguidify(self, &(PD_MSG_FIELD_I(affinity)));
        localDeguidify(self, &(PD_MSG_FIELD_I(currentEdt)));
        localDeguidify(self, &(PD_MSG_FIELD_I(parentLatch)));
        ocrFatGuid_t *outputEvent = NULL;
        if(PD_MSG_FIELD_IO(outputEvent.guid) == UNINITIALIZED_GUID) {
            outputEvent = &(PD_MSG_FIELD_IO(outputEvent));
        }
        ASSERT((PD_MSG_FIELD_I(workType) == EDT_USER_WORKTYPE) || (PD_MSG_FIELD_I(workType) == EDT_RT_WORKTYPE));
        DPRINTF(DEBUG_LVL_VVERB, "WORK_CREATE request from 0x%lx\n",
                msg->srcLocation);
        PD_MSG_FIELD_O(returnDetail) = ceCreateEdt(
            self, &(PD_MSG_FIELD_IO(guid)), PD_MSG_FIELD_I(templateGuid),
            &PD_MSG_FIELD_IO(paramc), PD_MSG_FIELD_I(paramv), &PD_MSG_FIELD_IO(depc),
            PD_MSG_FIELD_I(properties), PD_MSG_FIELD_I(affinity), outputEvent,
            (ocrTask_t*)(PD_MSG_FIELD_I(currentEdt).metaDataPtr), PD_MSG_FIELD_I(parentLatch));
        DPRINTF(DEBUG_LVL_VVERB, "WORK_CREATE response: GUID: 0x%lx\n",
                PD_MSG_FIELD_IO(guid.guid));
        returnCode = ceProcessResponse(self, msg, 0);
#undef PD_MSG
#undef PD_TYPE
        EXIT_PROFILE;
        break;
    }

    case PD_MSG_WORK_DESTROY: {
        START_PROFILE(pd_hc_WorkDestroy);
#define PD_MSG msg
#define PD_TYPE PD_MSG_WORK_DESTROY
        localDeguidify(self, &(PD_MSG_FIELD_I(guid)));
        ocrTask_t *task = (ocrTask_t*)PD_MSG_FIELD_I(guid.metaDataPtr);
        ASSERT(task);
        DPRINTF(DEBUG_LVL_VVERB, "WORK_DESTROY req/resp from 0x%lx for GUID 0x%lx\n",
                msg->srcLocation, PD_MSG_FIELD_I(guid.guid));
        ASSERT(task->fctId == self->taskFactories[0]->factoryId);
        PD_MSG_FIELD_O(returnDetail) = self->taskFactories[0]->fcts.destruct(task);
        returnCode = ceProcessResponse(self, msg, 0);
#undef PD_MSG
#undef PD_TYPE
        EXIT_PROFILE;
        break;
    }

    case PD_MSG_EDTTEMP_CREATE: {
        START_PROFILE(pd_ce_EdtTempCreate);
#define PD_MSG msg
#define PD_TYPE PD_MSG_EDTTEMP_CREATE
        DPRINTF(DEBUG_LVL_VVERB, "EDTTEMP_CREATE request from 0x%lx\n",
                msg->srcLocation);
#ifdef OCR_ENABLE_EDT_NAMING
        const char* edtName = PD_MSG_FIELD_I(funcName);
#else
        const char* edtName = "";
#endif
        PD_MSG_FIELD_O(returnDetail) = ceCreateEdtTemplate(
            self, &(PD_MSG_FIELD_IO(guid)), PD_MSG_FIELD_I(funcPtr), PD_MSG_FIELD_I(paramc),
            PD_MSG_FIELD_I(depc), edtName);
        DPRINTF(DEBUG_LVL_VVERB, "EDTTEMP_CREATE response: GUID: 0x%lx\n",
                PD_MSG_FIELD_IO(guid.guid));
        returnCode = ceProcessResponse(self, msg, 0);
#undef PD_MSG
#undef PD_TYPE
        EXIT_PROFILE;
        break;
    }

    case PD_MSG_EDTTEMP_DESTROY: {
        START_PROFILE(pd_ce_EdtTempDestroy);
#define PD_MSG msg
#define PD_TYPE PD_MSG_EDTTEMP_DESTROY
        localDeguidify(self, &(PD_MSG_FIELD_I(guid)));
        ocrTaskTemplate_t *tTemplate = (ocrTaskTemplate_t*)(PD_MSG_FIELD_I(guid.metaDataPtr));
        DPRINTF(DEBUG_LVL_VVERB, "EDTTEMP_DESTROY req/resp from 0x%lx for GUID 0x%lx\n",
                msg->srcLocation, PD_MSG_FIELD_I(guid.guid));
        ASSERT(tTemplate->fctId == self->taskTemplateFactories[0]->factoryId);
        PD_MSG_FIELD_O(returnDetail) = self->taskTemplateFactories[0]->fcts.destruct(tTemplate);
        returnCode = ceProcessResponse(self, msg, 0);
#undef PD_MSG
#undef PD_TYPE
        EXIT_PROFILE;
        break;
    }

    case PD_MSG_EVT_CREATE: {
        START_PROFILE(pd_ce_EvtCreate);
#define PD_MSG msg
#define PD_TYPE PD_MSG_EVT_CREATE
        ocrEventTypes_t type = PD_MSG_FIELD_I(type);
        DPRINTF(DEBUG_LVL_VVERB, "EVT_CREATE request from 0x%lx of type %u\n",
                msg->srcLocation, type);

        PD_MSG_FIELD_O(returnDetail) = ceCreateEvent(self, &(PD_MSG_FIELD_IO(guid)),
                                                     type, PD_MSG_FIELD_I(properties) & 1);
        DPRINTF(DEBUG_LVL_VVERB, "EVT_CREATE response for type %u: GUID: 0x%lx\n",
                type, PD_MSG_FIELD_IO(guid.guid));
        returnCode = ceProcessResponse(self, msg, 0);
#undef PD_MSG
#undef PD_TYPE
        EXIT_PROFILE;
        break;
    }

    case PD_MSG_EVT_DESTROY: {
        START_PROFILE(pd_ce_EvtDestroy);
#define PD_MSG msg
#define PD_TYPE PD_MSG_EVT_DESTROY
        localDeguidify(self, &(PD_MSG_FIELD_I(guid)));
        ocrEvent_t *evt = (ocrEvent_t*)PD_MSG_FIELD_I(guid.metaDataPtr);
        DPRINTF(DEBUG_LVL_VVERB, "EVT_DESTROY req/resp from 0x%lx for GUID 0x%lx\n",
                msg->srcLocation, PD_MSG_FIELD_I(guid.guid));
        ASSERT(evt->fctId == self->eventFactories[0]->factoryId);
        PD_MSG_FIELD_O(returnDetail) = self->eventFactories[0]->fcts[evt->kind].destruct(evt);
        returnCode = ceProcessResponse(self, msg, 0);
#undef PD_MSG
#undef PD_TYPE
        EXIT_PROFILE;
        break;
    }

    case PD_MSG_EVT_GET: {
        START_PROFILE(pd_ce_EvtGet);
#define PD_MSG msg
#define PD_TYPE PD_MSG_EVT_GET
        localDeguidify(self, &(PD_MSG_FIELD_I(guid)));
        ocrEvent_t *evt = (ocrEvent_t*)PD_MSG_FIELD_I(guid.metaDataPtr);
        DPRINTF(DEBUG_LVL_VVERB, "EVT_GET request from 0x%lx for GUID 0x%lx\n",
                msg->srcLocation, PD_MSG_FIELD_I(guid.guid));
        ASSERT(evt->fctId == self->eventFactories[0]->factoryId);
        PD_MSG_FIELD_O(data) = self->eventFactories[0]->fcts[evt->kind].get(evt);
        DPRINTF(DEBUG_LVL_VVERB, "EVT_GET response for GUID 0x%lx: GUID: 0x%lx\n",
                evt->guid, PD_MSG_FIELD_O(data.guid));
        returnCode = ceProcessResponse(self, msg, 0);
#undef PD_MSG
#undef PD_TYPE
        EXIT_PROFILE;
        break;
    }

    case PD_MSG_GUID_CREATE: {
        START_PROFILE(pd_ce_GuidCreate);
#define PD_MSG msg
#define PD_TYPE PD_MSG_GUID_CREATE
        if(PD_MSG_FIELD_I(size) != 0) {
            // Here we need to create a metadata area as well
            DPRINTF(DEBUG_LVL_VVERB, "GUID_CREATE(new) request from 0x%lx\n",
                    msg->srcLocation);
            PD_MSG_FIELD_O(returnDetail) = self->guidProviders[0]->fcts.createGuid(
                self->guidProviders[0], &(PD_MSG_FIELD_IO(guid)), PD_MSG_FIELD_I(size),
                PD_MSG_FIELD_I(kind));
        } else {
            // Here we just need to associate a GUID
            ocrGuid_t temp;
            DPRINTF(DEBUG_LVL_VVERB, "GUID_CREATE(exist) request from 0x%lx\n",
                    msg->srcLocation);
            PD_MSG_FIELD_O(returnDetail) = self->guidProviders[0]->fcts.getGuid(
                self->guidProviders[0], &temp, (u64)PD_MSG_FIELD_IO(guid.metaDataPtr),
                PD_MSG_FIELD_I(kind));
            PD_MSG_FIELD_IO(guid.guid) = temp;
        }
        DPRINTF(DEBUG_LVL_VVERB, "GUID_CREATE response: GUID: 0x%lx\n",
                PD_MSG_FIELD_IO(guid.guid));
        returnCode = ceProcessResponse(self, msg, 0);
#undef PD_MSG
#undef PD_TYPE
        EXIT_PROFILE;
        break;
    }

    case PD_MSG_GUID_INFO: {
        START_PROFILE(pd_ce_GuidInfo);
#define PD_MSG msg
#define PD_TYPE PD_MSG_GUID_INFO
        localDeguidify(self, &(PD_MSG_FIELD_IO(guid)));
        DPRINTF(DEBUG_LVL_VVERB, "GUID_INFO request from 0x%lx for GUID 0x%lx\n",
                msg->srcLocation, PD_MSG_FIELD_IO(guid.guid));
        if(PD_MSG_FIELD_I(properties) & KIND_GUIDPROP) {
            PD_MSG_FIELD_O(returnDetail) = self->guidProviders[0]->fcts.getKind(
                self->guidProviders[0], PD_MSG_FIELD_IO(guid.guid), &(PD_MSG_FIELD_O(kind)));
            if(PD_MSG_FIELD_O(returnDetail) == 0)
                PD_MSG_FIELD_O(returnDetail) = KIND_GUIDPROP | WMETA_GUIDPROP | RMETA_GUIDPROP;
        } else if (PD_MSG_FIELD_I(properties) & LOCATION_GUIDPROP) {
            PD_MSG_FIELD_O(returnDetail) = self->guidProviders[0]->fcts.getLocation(
                self->guidProviders[0], PD_MSG_FIELD_IO(guid.guid), &(PD_MSG_FIELD_O(location)));
            if(PD_MSG_FIELD_O(returnDetail) == 0) {
                PD_MSG_FIELD_O(returnDetail) = LOCATION_GUIDPROP | WMETA_GUIDPROP | RMETA_GUIDPROP;
            }
        } else {
            PD_MSG_FIELD_O(returnDetail) = WMETA_GUIDPROP | RMETA_GUIDPROP;
        }
        // TODO print more useful stuff
        DPRINTF(DEBUG_LVL_VVERB, "GUID_INFO response\n");
        returnCode = ceProcessResponse(self, msg, 0);
#undef PD_MSG
#undef PD_TYPE
        EXIT_PROFILE;
        break;
    }

    case PD_MSG_GUID_DESTROY: {
        START_PROFILE(pd_ce_GuidDestroy);
#define PD_MSG msg
#define PD_TYPE PD_MSG_GUID_DESTROY
        localDeguidify(self, &(PD_MSG_FIELD_I(guid)));
        DPRINTF(DEBUG_LVL_VVERB, "GUID_DESTROY req/resp from 0x%lx for GUID 0x%lx\n",
                msg->srcLocation, PD_MSG_FIELD_I(guid.guid));
        PD_MSG_FIELD_O(returnDetail) = self->guidProviders[0]->fcts.releaseGuid(
            self->guidProviders[0], PD_MSG_FIELD_I(guid), PD_MSG_FIELD_I(properties) & 1);
        returnCode = ceProcessResponse(self, msg, 0);
#undef PD_MSG
#undef PD_TYPE
        EXIT_PROFILE;
        break;
    }

    case PD_MSG_COMM_TAKE: {
        START_PROFILE(pd_ce_Take);
        // TAKE protocol on multiblock TG:
        //
        // When a XE is out of work it requests the CE for work.
        // If the CE is out of work, it pings a random neighbor CE for work.
        // So, a CE can receive a TAKE request from either a XE or another CE.
        // For a CE, the reaction to a TAKE request is currently the same,
        // whether it came from a XE or another CE, which is:
        //
        // Ask your own scheduler first.
        // If no work was found locally, start to ping other CE's for new work.
        // The protocol makes sure that an empty CE does not request back
        // to a requesting CE. It also makes sure (through local flags), that
        // it does not resend a request to another CE to which it had already
        // posted one earlier.
        //
        // This protocol may cause an exponentially growing number
        // of search messages until it fills up the system or finds work.
        // It may or may not be the fastest way to grab work, but definitely
        // not energy efficient nor communication-wise cheap. Also, who receives
        // what work is totally random.
        //
        // This implementation will serve as the initial baseline for future
        // improvements.
#define PD_MSG msg
#define PD_TYPE PD_MSG_COMM_TAKE
        ASSERT(PD_MSG_FIELD_IO(type) == OCR_GUID_EDT);
        // A TAKE request can come from either a CE or XE
        if (msg->type & PD_MSG_REQUEST) {
            DPRINTF(DEBUG_LVL_VVERB, "COMM_TAKE request from 0x%lx\n",
                msg->srcLocation);

            // If we get a request but the CE/XE doing the requesting hasn't requested
            // any specific guids, PD_MSG_FIELD_IO(guids) will be NULL so we set something up
            ocrFatGuid_t fguid[CE_TAKE_CHUNK_SIZE] = {{0}};
            if(PD_MSG_FIELD_IO(guids) == NULL) {
                PD_MSG_FIELD_IO(guids) = &(fguid[0]);
                ASSERT(PD_MSG_FIELD_IO(guidCount) <= CE_TAKE_CHUNK_SIZE);
            }
            u32 msgProperties = PD_MSG_FIELD_I(properties);
            //First check if my own scheduler can give out work
            PD_MSG_FIELD_O(returnDetail) = self->schedulers[0]->fcts.takeEdt(
                self->schedulers[0], &(PD_MSG_FIELD_IO(guidCount)), PD_MSG_FIELD_IO(guids));

            //If my scheduler does not have work, (i.e guidCount == 0)
            //then we need to start looking for work on other CE's.
            ocrPolicyDomainCe_t * cePolicy = (ocrPolicyDomainCe_t*)self;
            static s32 throttlecount = 1;
#ifndef HAL_FSIM_CE   // FIXME: trac #274
            if ((PD_MSG_FIELD_IO(guidCount) == 0) &&
                (cePolicy->shutdownMode == false) &&
                (--throttlecount <= 0)) {
            throttlecount = 200 * (self->neighborCount-1);
            u32 i;
            for (i = 0; i < self->neighborCount; i++) {
                if ((cePolicy->ceCommTakeActive[i] == 0) &&
                   (msg->srcLocation != self->neighbors[i])) {
#else
            static u32 i = 0;
            // FIXME: very basic self-throttling mechanism bug #268
            if ((PD_MSG_FIELD_IO(guidCount) == 0) &&
                (cePolicy->shutdownMode == false) &&
                (--throttlecount <= 0)) {

                throttlecount = 20*(self->neighborCount-1);
                //Try other CE's
                //Check if I already have an active work request pending on a CE
                //If not, then we post one
                if (self->neighborCount &&
                   (cePolicy->ceCommTakeActive[i] == 0) &&
                   (msg->srcLocation != self->neighbors[i])) {
#endif
#undef PD_MSG
#define PD_MSG (&ceMsg)
                        PD_MSG_STACK(ceMsg);
                        getCurrentEnv(NULL, NULL, NULL, &ceMsg);
                        ocrFatGuid_t fguid[CE_TAKE_CHUNK_SIZE] = {{0}};
                        ceMsg.destLocation = self->neighbors[i];
                        ceMsg.seqId = i;
                        ceMsg.type = PD_MSG_COMM_TAKE | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE | PD_CE_CE_MESSAGE;
                        PD_MSG_FIELD_IO(guids) = &(fguid[0]);
                        PD_MSG_FIELD_IO(guidCount) = CE_TAKE_CHUNK_SIZE;
                        PD_MSG_FIELD_I(properties) = 0;
                        PD_MSG_FIELD_IO(type) = OCR_GUID_EDT;
                        if(self->fcts.sendMessage(self, ceMsg.destLocation, &ceMsg, NULL, 0) == 0)
                            cePolicy->ceCommTakeActive[i] = 1;
#undef PD_MSG
#define PD_MSG msg
                    }
#ifdef HAL_FSIM_CE // FIXME: trac #274
                if(++i >= self->neighborCount) i = 0;
#else
                }
#endif
            }

            // Respond to the requester
            // If my own scheduler had extra work, we respond with work
            // If not, then we respond with no work (but we've already put out work requests by now)
            if(PD_MSG_FIELD_IO(guidCount)) {
                  if(PD_MSG_FIELD_IO(guids[0]).metaDataPtr == NULL)
                      localDeguidify(self, &(PD_MSG_FIELD_IO(guids[0])));
            }
            returnCode = ceProcessResponse(self, msg, msgProperties);
        } else { // A TAKE response has to be from another CE responding to my own request
            ASSERT(msg->type & PD_MSG_RESPONSE);
            u32 i, j;
            for (i = 0; i < self->neighborCount; i++) {
                if (msg->srcLocation == self->neighbors[i]) {
                    ocrPolicyDomainCe_t * cePolicy = (ocrPolicyDomainCe_t*)self;
                    cePolicy->ceCommTakeActive[i] = 0;
                    if (PD_MSG_FIELD_IO(guidCount) > 0) {
                        //If my work request was responded with work,
                        //then we store it in our own scheduler
                        ASSERT(PD_MSG_FIELD_IO(type) == OCR_GUID_EDT);
                        ocrFatGuid_t *fEdtGuids = PD_MSG_FIELD_IO(guids);

                        DPRINTF(DEBUG_LVL_INFO, "Received %lu Edt(s) from 0x%lx: ",
                                PD_MSG_FIELD_IO(guidCount), (u64)msg->srcLocation);
                        for (j = 0; j < PD_MSG_FIELD_IO(guidCount); j++) {
                            DPRINTF(DEBUG_LVL_INFO, "(guid:0x%lx metadata: %p) ", fEdtGuids[j].guid, fEdtGuids[j].metaDataPtr);
                        }
                        DPRINTF(DEBUG_LVL_INFO, "\n");

                        PD_MSG_FIELD_O(returnDetail) = self->schedulers[0]->fcts.giveEdt(
                            self->schedulers[0], &(PD_MSG_FIELD_IO(guidCount)), fEdtGuids);
                    }
                    break;
                }
            }
            ASSERT(i < self->neighborCount);
        }
#undef PD_MSG
#undef PD_TYPE
        EXIT_PROFILE;
        break;
    }

    case PD_MSG_COMM_GIVE: {
        START_PROFILE(pd_ce_Give);
#define PD_MSG msg
#define PD_TYPE PD_MSG_COMM_GIVE
        ASSERT(PD_MSG_FIELD_I(type) == OCR_GUID_EDT);
        DPRINTF(DEBUG_LVL_VVERB, "COMM_GIVE req/resp from 0x%lx with GUID 0x%lx\n",
                msg->srcLocation, (PD_MSG_FIELD_IO(guids))->guid);
        PD_MSG_FIELD_O(returnDetail) = self->schedulers[0]->fcts.giveEdt(
            self->schedulers[0], &(PD_MSG_FIELD_IO(guidCount)),
            PD_MSG_FIELD_IO(guids));
        returnCode = ceProcessResponse(self, msg, 0);
#undef PD_MSG
#undef PD_TYPE
        EXIT_PROFILE;
        break;
    }

    case PD_MSG_DEP_ADD: {
        START_PROFILE(pd_ce_DepAdd);
#define PD_MSG msg
#define PD_TYPE PD_MSG_DEP_ADD
        DPRINTF(DEBUG_LVL_VVERB, "DEP_ADD req/resp from 0x%lx for 0x%lx -> 0x%lx\n",
                msg->srcLocation, PD_MSG_FIELD_I(source.guid), PD_MSG_FIELD_I(dest.guid));
        // We first get information about the source and destination
        ocrGuidKind srcKind, dstKind;
        self->guidProviders[0]->fcts.getVal(
            self->guidProviders[0], PD_MSG_FIELD_I(source.guid),
            (u64*)(&(PD_MSG_FIELD_I(source.metaDataPtr))), &srcKind);
        self->guidProviders[0]->fcts.getVal(
            self->guidProviders[0], PD_MSG_FIELD_I(dest.guid),
            (u64*)(&(PD_MSG_FIELD_I(dest.metaDataPtr))), &dstKind);

        ocrFatGuid_t src = PD_MSG_FIELD_I(source);
        ocrFatGuid_t dest = PD_MSG_FIELD_I(dest);
        ocrDbAccessMode_t mode = (PD_MSG_FIELD_IO(properties) & DB_PROP_MODE_MASK); //lower 3-bits is mode //TODO not pretty
        if (srcKind == NULL_GUID) {
            //NOTE: Handle 'NULL_GUID' case here to be safe although
            //we've already caught it in ocrAddDependence for performance
            // This is equivalent to an immediate satisfy
            PD_MSG_FIELD_O(returnDetail) = convertDepAddToSatisfy(
                self, src, dest, PD_MSG_FIELD_I(slot));
        } else if (srcKind == OCR_GUID_DB) {
            if(dstKind & OCR_GUID_EVENT) {
                // This is equivalent to an immediate satisfy
                PD_MSG_FIELD_O(returnDetail) = convertDepAddToSatisfy(
                    self, src, dest, PD_MSG_FIELD_I(slot));
            } else {
                // NOTE: We could use convertDepAddToSatisfy since adding a DB dependence
                // is equivalent to satisfy. However, we want to go through the register
                // function to make sure the access mode is recorded.
                ASSERT(dstKind == OCR_GUID_EDT);
                PD_MSG_STACK(registerMsg);
                getCurrentEnv(NULL, NULL, NULL, &registerMsg);
                ocrFatGuid_t sourceGuid = PD_MSG_FIELD_I(source);
                ocrFatGuid_t destGuid = PD_MSG_FIELD_I(dest);
                u32 slot = PD_MSG_FIELD_I(slot);
#undef PD_MSG
#undef PD_TYPE
#define PD_MSG (&registerMsg)
#define PD_TYPE PD_MSG_DEP_REGSIGNALER
                registerMsg.type = PD_MSG_DEP_REGSIGNALER | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
                // Registers sourceGuid (signaler) onto destGuid
                PD_MSG_FIELD_I(signaler) = sourceGuid;
                PD_MSG_FIELD_I(dest) = destGuid;
                PD_MSG_FIELD_I(slot) = slot;
                PD_MSG_FIELD_I(mode) = mode;
                PD_MSG_FIELD_I(properties) = true; // Specify context is add-dependence
                RESULT_PROPAGATE(self->fcts.processMessage(self, &registerMsg, true));
#undef PD_MSG
#undef PD_TYPE
#define PD_MSG msg
#define PD_TYPE PD_MSG_DEP_ADD
            }
        } else {
            if(srcKind & OCR_GUID_EVENT) {
                ocrEvent_t *evt = (ocrEvent_t*)(src.metaDataPtr);
                ASSERT(evt->fctId == self->eventFactories[0]->factoryId);
                self->eventFactories[0]->fcts[evt->kind].registerWaiter(
                    evt, dest, PD_MSG_FIELD_I(slot), true);
            } else {
                // Some sanity check
                ASSERT(srcKind == OCR_GUID_EDT);
            }
            if(dstKind == OCR_GUID_EDT) {
                ocrTask_t *task = (ocrTask_t*)(dest.metaDataPtr);
                ASSERT(task->fctId == self->taskFactories[0]->factoryId);
                self->taskFactories[0]->fcts.registerSignaler(task, src, PD_MSG_FIELD_I(slot),
                    PD_MSG_FIELD_IO(properties) & DB_PROP_MODE_MASK, true);
            } else if(dstKind & OCR_GUID_EVENT) {
                ocrEvent_t *evt = (ocrEvent_t*)(dest.metaDataPtr);
                ASSERT(evt->fctId == self->eventFactories[0]->factoryId);
                self->eventFactories[0]->fcts[evt->kind].registerSignaler(
                    evt, src, PD_MSG_FIELD_I(slot), PD_MSG_FIELD_IO(properties) & DB_PROP_MODE_MASK, true);
            } else {
                ASSERT(0); // Cannot have other types of destinations
            }
        }

#ifdef OCR_ENABLE_STATISTICS
        // TODO: Fixme
        statsDEP_ADD(pd, getCurrentEDT(), NULL, signalerGuid, waiterGuid, NULL, slot);
#endif
        returnCode = ceProcessResponse(self, msg, 0);
#undef PD_MSG
#undef PD_TYPE
        EXIT_PROFILE;
        break;
    }

    case PD_MSG_DEP_REGSIGNALER: {
        START_PROFILE(pd_ce_DepRegSignaler);
#define PD_MSG msg
#define PD_TYPE PD_MSG_DEP_REGSIGNALER
        // We first get information about the signaler and destination
        DPRINTF(DEBUG_LVL_VVERB, "DEP_REGSIGNALER req/resp from 0x%lx for 0x%lx -> 0x%lx\n",
                msg->srcLocation, PD_MSG_FIELD_I(signaler.guid), PD_MSG_FIELD_I(dest.guid));
        ocrGuidKind signalerKind, dstKind;
        self->guidProviders[0]->fcts.getVal(
            self->guidProviders[0], PD_MSG_FIELD_I(signaler.guid),
            (u64*)(&(PD_MSG_FIELD_I(signaler.metaDataPtr))), &signalerKind);
        self->guidProviders[0]->fcts.getVal(
            self->guidProviders[0], PD_MSG_FIELD_I(dest.guid),
            (u64*)(&(PD_MSG_FIELD_I(dest.metaDataPtr))), &dstKind);

        ocrFatGuid_t signaler = PD_MSG_FIELD_I(signaler);
        ocrFatGuid_t dest = PD_MSG_FIELD_I(dest);
        bool isAddDep = PD_MSG_FIELD_I(properties);

        if (dstKind & OCR_GUID_EVENT) {
            ocrEvent_t *evt = (ocrEvent_t*)(dest.metaDataPtr);
            ASSERT(evt->fctId == self->eventFactories[0]->factoryId);
            self->eventFactories[0]->fcts[evt->kind].registerSignaler(
                evt, signaler, PD_MSG_FIELD_I(slot), PD_MSG_FIELD_I(mode), isAddDep);
        } else if(dstKind == OCR_GUID_EDT) {
            ocrTask_t *edt = (ocrTask_t*)(dest.metaDataPtr);
            ASSERT(edt->fctId == self->taskFactories[0]->factoryId);
            self->taskFactories[0]->fcts.registerSignaler(
                edt, signaler, PD_MSG_FIELD_I(slot), PD_MSG_FIELD_I(mode), isAddDep);
        } else {
            ASSERT(0); // No other things we can register signalers on
        }
#ifdef OCR_ENABLE_STATISTICS
        // TODO: Fixme
        statsDEP_ADD(pd, getCurrentEDT(), NULL, signalerGuid, waiterGuid, NULL, slot);
#endif
        returnCode = ceProcessResponse(self, msg, 0);
#undef PD_MSG
#undef PD_TYPE
        EXIT_PROFILE;
        break;
    }

    case PD_MSG_DEP_REGWAITER: {
        START_PROFILE(pd_ce_DepRegWaiter);
#define PD_MSG msg
#define PD_TYPE PD_MSG_DEP_REGWAITER
        // We first get information about the signaler and destination
        DPRINTF(DEBUG_LVL_VVERB, "DEP_REGWAITER req/resp from 0x%lx for 0x%lx -> 0x%lx\n",
                msg->srcLocation, PD_MSG_FIELD_I(dest.guid), PD_MSG_FIELD_I(waiter.guid));
        ocrGuidKind waiterKind, dstKind;
        self->guidProviders[0]->fcts.getVal(
            self->guidProviders[0], PD_MSG_FIELD_I(waiter.guid),
            (u64*)(&(PD_MSG_FIELD_I(waiter.metaDataPtr))), &waiterKind);
        self->guidProviders[0]->fcts.getVal(
            self->guidProviders[0], PD_MSG_FIELD_I(dest.guid),
            (u64*)(&(PD_MSG_FIELD_I(dest.metaDataPtr))), &dstKind);

        ocrFatGuid_t waiter = PD_MSG_FIELD_I(waiter);
        ocrFatGuid_t dest = PD_MSG_FIELD_I(dest);

        ASSERT(dstKind & OCR_GUID_EVENT); // Waiters can only wait on events
        ocrEvent_t *evt = (ocrEvent_t*)(dest.metaDataPtr);
        ASSERT(evt->fctId == self->eventFactories[0]->factoryId);
        self->eventFactories[0]->fcts[evt->kind].registerWaiter(
            evt, waiter, PD_MSG_FIELD_I(slot), false);
#ifdef OCR_ENABLE_STATISTICS
        // TODO: Fixme
        statsDEP_ADD(pd, getCurrentEDT(), NULL, signalerGuid, waiterGuid, NULL, slot);
#endif
        returnCode = ceProcessResponse(self, msg, 0);
#undef PD_MSG
#undef PD_TYPE
        EXIT_PROFILE;
        break;
    }

    case PD_MSG_DEP_SATISFY: {
        START_PROFILE(pd_ce_DepSatisfy);
#define PD_MSG msg
#define PD_TYPE PD_MSG_DEP_SATISFY
        DPRINTF(DEBUG_LVL_VVERB, "DEP_SATISFY from 0x%lx for GUID 0x%lx with 0x%lx\n",
                msg->srcLocation, PD_MSG_FIELD_I(guid.guid), PD_MSG_FIELD_I(payload.guid));
        ocrGuidKind dstKind;
        self->guidProviders[0]->fcts.getVal(
            self->guidProviders[0], PD_MSG_FIELD_I(guid.guid),
            (u64*)(&(PD_MSG_FIELD_I(guid.metaDataPtr))), &dstKind);

        ocrFatGuid_t dst = PD_MSG_FIELD_I(guid);
        if(dstKind & OCR_GUID_EVENT) {
            ocrEvent_t *evt = (ocrEvent_t*)(dst.metaDataPtr);
            ASSERT(evt->fctId == self->eventFactories[0]->factoryId);
            self->eventFactories[0]->fcts[evt->kind].satisfy(
                evt, PD_MSG_FIELD_I(payload), PD_MSG_FIELD_I(slot));
        } else {
            if(dstKind == OCR_GUID_EDT) {
                ocrTask_t *edt = (ocrTask_t*)(dst.metaDataPtr);
                ASSERT(edt->fctId == self->taskFactories[0]->factoryId);
                self->taskFactories[0]->fcts.satisfy(
                    edt, PD_MSG_FIELD_I(payload), PD_MSG_FIELD_I(slot));
            } else {
                ASSERT(0); // We can't satisfy anything else
            }
        }
#ifdef OCR_ENABLE_STATISTICS
        // TODO: Fixme
        statsDEP_ADD(pd, getCurrentEDT(), NULL, signalerGuid, waiterGuid, NULL, slot);
#endif
        returnCode = ceProcessResponse(self, msg, 0);
#undef PD_MSG
#undef PD_TYPE
        EXIT_PROFILE;
        break;
    }
#if 0
    case 0: // FIXME: Treat null messages as shutdown, till #134 is fixed
    case PD_MSG_MGT_SHUTDOWN: {
        START_PROFILE(pd_ce_Shutdown);
#define PD_MSG msg
#define PD_TYPE PD_MSG_MGT_SHUTDOWN
        //u32 neighborCount = self->neighborCount;
        ocrPolicyDomainCe_t * cePolicy = (ocrPolicyDomainCe_t *)self;
        if (cePolicy->shutdownMode == false) {
            cePolicy->shutdownMode = true;
        }
        if (msg->srcLocation == self->myLocation && self->myLocation != self->parentLocation) {
            msg->destLocation = self->parentLocation;
            RESULT_ASSERT(self->fcts.sendMessage(self, msg->destLocation, msg, NULL, BLOCKING_SEND_MSG_PROP), ==, 0);
        } else {
            if (msg->type & PD_MSG_REQUEST) {
                // This triggers the shutdown of the machine
                ASSERT(!(msg->type & PD_MSG_RESPONSE));
                ASSERT(msg->srcLocation != self->myLocation);
                if (self->shutdownCode == 0)
                    self->shutdownCode = PD_MSG_FIELD_I(errorCode);
#ifdef HAL_FSIM_CE
                if(msg->srcLocation != self->parentLocation)
#endif
                cePolicy->shutdownCount++;
                DPRINTF (DEBUG_LVL_INFO, "MSG_SHUTDOWN REQ from Agent 0x%lx; shutdown %u/%u\n",
                    msg->srcLocation, cePolicy->shutdownCount, cePolicy->shutdownMax);
            } else {
                ASSERT(msg->type & PD_MSG_RESPONSE);
                DPRINTF (DEBUG_LVL_INFO, "MSG_SHUTDOWN RESP from Agent 0x%lx; shutdown %u/%u\n",
                    msg->srcLocation, cePolicy->shutdownCount, cePolicy->shutdownMax);
            }

            if (cePolicy->shutdownCount == cePolicy->shutdownMax) {
                //TODO not sure about this for CE
                self->fcts.stop(self, RL_STOP, RL_ACTION_ENTER);
                if (self->myLocation != self->parentLocation) {
                    ocrShutdown();
                }
            }
        }
#undef PD_MSG
#undef PD_TYPE
        EXIT_PROFILE;
        break;
    }

    case PD_MSG_MGT_FINISH: {
        ASSERT(false && "Implement runlevel for CE/XE")
        // self->fcts.finish(self);
        returnCode = ceProcessResponse(self, msg, 0);
        break;
    }
#endif

    case PD_MSG_SAL_PRINT: {
        DPRINTF(DEBUG_LVL_WARN, "CE PD does not yet implement PRINT (FIXME)\n");
        ASSERT(0);
    }

    case PD_MSG_SAL_READ: {
        DPRINTF(DEBUG_LVL_WARN, "CE PD does not yet implement READ (FIXME)\n");
        ASSERT(0);
    }

    case PD_MSG_SAL_WRITE: {
        DPRINTF(DEBUG_LVL_WARN, "CE PD does not yet implement WRITE (FIXME)\n");
        ASSERT(0);
    }

    default:
        // Not handled
        DPRINTF(DEBUG_LVL_WARN, "Unknown message type 0x%x\n",
                msg->type & PD_MSG_TYPE_ONLY);
        ASSERT(0);
    }
    return returnCode;
}

u8 cePdSendMessage(ocrPolicyDomain_t* self, ocrLocation_t target, ocrPolicyMsg_t *message,
                   ocrMsgHandle_t **handle, u32 properties) {
    return self->commApis[0]->fcts.sendMessage(self->commApis[0], target, message, handle, properties);
}

u8 cePdPollMessage(ocrPolicyDomain_t *self, ocrMsgHandle_t **handle) {
    return self->commApis[0]->fcts.pollMessage(self->commApis[0], handle);
}

u8 cePdWaitMessage(ocrPolicyDomain_t *self,  ocrMsgHandle_t **handle) {
    return self->commApis[0]->fcts.waitMessage(self->commApis[0], handle);
}

void* cePdMalloc(ocrPolicyDomain_t *self, u64 size) {
    void* result;
    u64 engineIndex = getEngineIndex(self, self->myLocation);
//TODO: Unlike other datablock allocation contexts, cePdMalloc is not going to get hints, so this part can be stripped/simplified. Maybe use an order value directly hard-coded in this function.
    u64 prescription = PRESCRIPTION;
    u64 allocatorHints;  // Allocator hint
    u64 levelIndex; // "Level" of the memory hierarchy, taken from the "prescription" to pick from the allocators array the allocator to try.
    s8 allocatorIndex;
    do {
        allocatorHints = (prescription & PRESCRIPTION_HINT_MASK) ?
            (OCR_ALLOC_HINT_NONE) :              // If the hint is non-zero, pick this (e.g. for tlsf, tries "remnant" pool.)
            (OCR_ALLOC_HINT_REDUCE_CONTENTION);  // If the hint is zero, pick this (e.g. for tlsf, tries a round-robin-chosen "slice" pool.)
        prescription >>= PRESCRIPTION_HINT_NUMBITS;
        levelIndex = prescription & PRESCRIPTION_LEVEL_MASK;
        prescription >>= PRESCRIPTION_LEVEL_NUMBITS;
        allocatorIndex = self->allocatorIndexLookup[engineIndex*NUM_MEM_LEVELS_SUPPORTED+levelIndex]; // Lookup index of allocator to use for requesting engine (aka agent) at prescribed memory hierarchy level.
        if ((allocatorIndex < 0) ||
            (allocatorIndex >= self->allocatorCount) ||
            (self->allocators[allocatorIndex] == NULL)) continue;  // Skip this allocator if it doesn't exist.
        result = self->allocators[allocatorIndex]->fcts.allocate(self->allocators[allocatorIndex], size, allocatorHints);
        if (result) {
            return result;
        }
    } while (prescription != 0);

    DPRINTF(DEBUG_LVL_WARN, "cePdMalloc returning NULL for size %ld\n", (u64) size);
    return NULL;
}

void cePdFree(ocrPolicyDomain_t *self, void* addr) {
#if 0 // defined(HAL_FSIM_CE)
    u64 base = DR_CE_BASE(CHIP_FROM_ID(self->myLocation),
                          UNIT_FROM_ID(self->myLocation),
                          BLOCK_FROM_ID(self->myLocation));
    if(((u64)addr < CE_MSR_BASE) || ((base & (u64)addr) == base)) {  // trac #222
        allocatorFreeFunction(addr);
    } else {
        PD_MSG_STACK(msg);
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_MEM_UNALLOC
        msg.type = PD_MSG_MEM_UNALLOC | PD_MSG_REQUEST;
        msg.destLocation = MAKE_CORE_ID(0,
                                        0,
                                        ((((u64)addr >> MAP_CHIP_SHIFT) & ((1ULL<<MAP_CHIP_LEN) - 1)) - 1),
                                        ((((u64)addr >> MAP_UNIT_SHIFT) & ((1ULL<<MAP_UNIT_LEN) - 1)) - 2),
                                        ((((u64)addr >> MAP_BLOCK_SHIFT) & ((1ULL<<MAP_BLOCK_LEN) - 1)) - 2),
                                        ID_AGENT_CE);
        msg.srcLocation = self->myLocation;
        PD_MSG_FIELD_I(allocatingPD.guid) = NULL_GUID;
        PD_MSG_FIELD_I(allocatingPD.metaDataPtr) = NULL;
        PD_MSG_FIELD_I(allocator.guid) = NULL_GUID;
        PD_MSG_FIELD_I(allocator.metaDataPtr) = NULL;
        PD_MSG_FIELD_I(ptr) = ((void *) addr);
        // TODO: Hun? What is this? 0 shouldn't be a valid type...
        PD_MSG_FIELD_I(type) = 0;
        while(self->fcts.sendMessage(self, msg.destLocation, &msg, NULL, 0)) {
           PD_MSG_STACK(msg);
           ocrMsgHandle_t myHandle;
           myHandle.msg = &msg;
           ocrMsgHandle_t *handle = &myHandle;
           while(!self->fcts.pollMessage(self, &handle))
               RESULT_ASSERT(self->fcts.processMessage(self, handle->response, true), ==, 0);
        }
#undef PD_MSG
#undef PD_TYPE
    }
#else
    allocatorFreeFunction(addr);
#endif
}

ocrPolicyDomain_t * newPolicyDomainCe(ocrPolicyDomainFactory_t * factory,
#ifdef OCR_ENABLE_STATISTICS
                                      ocrStats_t *statsObject,
#endif
                                      ocrCost_t *costFunction, ocrParamList_t *perInstance) {

    ocrPolicyDomainCe_t * derived = (ocrPolicyDomainCe_t *) runtimeChunkAlloc(sizeof(ocrPolicyDomainCe_t), PERSISTENT_CHUNK);
    ocrPolicyDomain_t * base = (ocrPolicyDomain_t *) derived;

    ASSERT(base);

#ifdef OCR_ENABLE_STATISTICS
    factory->initialize(factory, base, statsObject, costFunction, perInstance);
#else
    factory->initialize(factory, base, costFunction, perInstance);
#endif

    return base;
}

void initializePolicyDomainCe(ocrPolicyDomainFactory_t * factory, ocrPolicyDomain_t* self,
#ifdef OCR_ENABLE_STATISTICS
                              ocrStats_t *statObject,
#endif
                              ocrCost_t *costFunction, ocrParamList_t *perInstance) {
    u64 i;
#ifdef OCR_ENABLE_STATISTICS
    self->statsObject = statsObject;
#endif

    initializePolicyDomainOcr(factory, self, perInstance);
    ocrPolicyDomainCe_t* derived = (ocrPolicyDomainCe_t*) self;

    derived->xeCount = ((paramListPolicyDomainCeInst_t*)perInstance)->xeCount;
    self->neighborCount = ((paramListPolicyDomainCeInst_t*)perInstance)->neighborCount;
    self->allocatorIndexLookup = (s8 *) runtimeChunkAlloc((derived->xeCount+1) * NUM_MEM_LEVELS_SUPPORTED, PERSISTENT_CHUNK);
    for (i = 0; i < ((derived->xeCount+1) * NUM_MEM_LEVELS_SUPPORTED); i++) self->allocatorIndexLookup[i] = -1;

    derived->shutdownCount = 0;
    derived->shutdownMax = 0;
    derived->shutdownMode = false;

    derived->ceCommTakeActive = (bool*)runtimeChunkAlloc(sizeof(bool) *  self->neighborCount, 0);
}

static void destructPolicyDomainFactoryCe(ocrPolicyDomainFactory_t * factory) {
    runtimeChunkFree((u64)factory, NULL);
}

ocrPolicyDomainFactory_t * newPolicyDomainFactoryCe(ocrParamList_t *perType) {
    ocrPolicyDomainFactory_t* base = (ocrPolicyDomainFactory_t*) runtimeChunkAlloc(sizeof(ocrPolicyDomainFactoryCe_t), NONPERSISTENT_CHUNK);
    ASSERT(base); // Check allocation

    // Set factory's methods
#ifdef OCR_ENABLE_STATISTICS
    base->instantiate = FUNC_ADDR(ocrPolicyDomain_t*(*)(ocrPolicyDomainFactory_t*,ocrStats_t*,
                                  ocrCost_t *,ocrParamList_t*), newPolicyDomainCe);
    base->initialize = FUNC_ADDR(void(*)(ocrPolicyDomainFactory_t*,ocrPolicyDomain_t*,
                                         ocrStats_t*,ocrCost_t *,ocrParamList_t*), initializePolicyDomainCe);
#endif
    base->instantiate = &newPolicyDomainCe;
    base->initialize = &initializePolicyDomainCe;
    base->destruct = &destructPolicyDomainFactoryCe;

    // Set future PDs' instance  methods
    base->policyDomainFcts.destruct = FUNC_ADDR(void(*)(ocrPolicyDomain_t*), cePolicyDomainDestruct);
    base->policyDomainFcts.switchRunlevel = FUNC_ADDR(u8 (*)(ocrPolicyDomain_t*, ocrRunlevel_t, u32), cePdSwitchRunlevel);
    base->policyDomainFcts.processMessage = FUNC_ADDR(u8(*)(ocrPolicyDomain_t*,ocrPolicyMsg_t*,u8), cePolicyDomainProcessMessage);

    base->policyDomainFcts.sendMessage = FUNC_ADDR(u8 (*)(ocrPolicyDomain_t*, ocrLocation_t,
                                                          ocrPolicyMsg_t *, ocrMsgHandle_t**, u32),
                                                   cePdSendMessage);
    base->policyDomainFcts.pollMessage = FUNC_ADDR(u8 (*)(ocrPolicyDomain_t*, ocrMsgHandle_t**), cePdPollMessage);
    base->policyDomainFcts.waitMessage = FUNC_ADDR(u8 (*)(ocrPolicyDomain_t*, ocrMsgHandle_t**), cePdWaitMessage);

    base->policyDomainFcts.pdMalloc = FUNC_ADDR(void*(*)(ocrPolicyDomain_t*,u64), cePdMalloc);
    base->policyDomainFcts.pdFree = FUNC_ADDR(void(*)(ocrPolicyDomain_t*,void*), cePdFree);
#ifdef OCR_ENABLE_STATISTICS
    base->policyDomainFcts.getStats = FUNC_ADDR(ocrStats_t*(*)(ocrPolicyDomain_t*),ceGetStats);
#endif

    return base;
}

#endif /* ENABLE_POLICY_DOMAIN_CE */
