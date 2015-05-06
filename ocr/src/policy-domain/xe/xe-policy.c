/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#include "ocr-config.h"
#ifdef ENABLE_POLICY_DOMAIN_XE

#include "debug.h"
#include "ocr-errors.h"
#include "ocr-policy-domain.h"
#include "ocr-sysboot.h"
#include "ocr-runtime-types.h"

#ifdef OCR_ENABLE_STATISTICS
#include "ocr-statistics.h"
#endif

#include "policy-domain/xe/xe-policy.h"

#ifdef ENABLE_SYSBOOT_FSIM
#include "rmd-bin-files.h"
#endif

#include "utils/profiler/profiler.h"

#define DEBUG_TYPE POLICY


void xePolicyDomainStart(ocrPolicyDomain_t * policy);

extern void ocrShutdown(void);

// Function to cause run-level switches in this PD
u8 xePdSwitchRunlevel(ocrPolicyDomain_t *policy, ocrRunlevel_t runlevel, u32 properties) {
    s32 i=0, j, curPhase, phaseCount;
    u32 maxCount;

    u8 toReturn = 0;
    u32 origProperties = properties;
    u32 propertiesPreComputes = properties;

#define GET_PHASE(counter) curPhase = (properties & RL_BRING_UP)?counter:(phaseCount - counter - 1)

    ocrPolicyDomainXe_t* rself = (ocrPolicyDomainXe_t*)policy;
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
void xePolicyDomainBegin(ocrPolicyDomain_t * policy) {
    DPRINTF(DEBUG_LVL_VVERB, "xePolicyDomainBegin called on policy at 0x%lx\n", (u64) policy);
#ifdef ENABLE_SYSBOOT_FSIM
    if (XE_PDARGS_OFFSET != offsetof(ocrPolicyDomainXe_t, packedArgsLocation)) {
        DPRINTF(DEBUG_LVL_WARN, "XE_PDARGS_OFFSET (in .../ss/common/include/rmd-bin-files.h) is 0x%lx.  Should be 0x%lx\n",
            (u64) XE_PDARGS_OFFSET, (u64) offsetof(ocrPolicyDomainXe_t, packedArgsLocation));
        ASSERT (0);
    }
#endif
    // The PD should have been brought up by now and everything instantiated

    u64 i = 0;
    u64 maxCount = 0;

    ((ocrPolicyDomainXe_t*)policy)->shutdownInitiated = 0;

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

    maxCount = policy->commApiCount;
    for(i = 0; i < maxCount; i++) {
        policy->commApis[i]->fcts.begin(policy->commApis[i], policy);
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

#if defined(SAL_FSIM_XE) // If running on FSim XE
    xePolicyDomainStart(policy);
    hal_exit(0);
#endif

#ifdef TEMPORARY_FSIM_HACK_TILL_WE_FIGURE_OCR_START_STOP_HANDSHAKES
    hal_exit(0);
#endif
}

void xePolicyDomainStart(ocrPolicyDomain_t * policy) {
    DPRINTF(DEBUG_LVL_VVERB, "xePolicyDomainStart called on policy at 0x%lx\n", (u64) policy);
    // The PD should have been brought up by now and everything instantiated
    // This is a bit ugly but I can't find a cleaner solution:
    //   - we need to associate the environment with the
    //   currently running worker/PD so that we can use getCurrentEnv

    u64 i = 0;
    u64 maxCount = 0;

    maxCount = policy->guidProviderCount;
    for(i = 0; i < maxCount; ++i) {
        policy->guidProviders[i]->fcts.start(policy->guidProviders[i], policy);
    }

    guidify(policy, (u64)policy, &(policy->fguid), OCR_GUID_POLICY);

    /*maxCount = policy->allocatorCount;
    for(i = 0; i < maxCount; ++i) {
        policy->allocators[i]->fcts.start(policy->allocators[i], policy);
    }

    maxCount = policy->schedulerCount;
    for(i = 0; i < maxCount; ++i) {
        policy->schedulers[i]->fcts.start(policy->schedulers[i], policy);
    }*/

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

void xePolicyDomainStop(ocrPolicyDomain_t * policy, ocrRunLevel_t expectedRl, ocrRunLevel_t newRl) {
    switch(newRl) {
        case RL_STOP: {
            // Finish everything in reverse order
            // In XE, we MUST call stop on the master worker first.
            // The master worker enters its work routine loop and will
            // be unlocked by ocrShutdown
            u64 i = 0;
            u64 maxCount = 0;

            // Note: As soon as worker '0' is stopped; its thread is
            // free to fall-through and continue shutting down the
            // policy domain
            maxCount = policy->workerCount;
            for(i = 0; i < maxCount; i++) {
                policy->workers[i]->fcts.stop(policy->workers[i], newRl);
            }
            // WARNING: Do not add code here unless you know what you're doing !!
            // If we are here, it means an EDT called ocrShutdown which
            // logically finished workers and can make thread '0' executes this
            // code before joining the other threads.

            // Thread '0' joins the other (N-1) threads.

            maxCount = policy->commApiCount;
            for(i = 0; i < maxCount; i++) {
                policy->commApis[i]->fcts.stop(policy->commApis[i], newRl);
            }

            maxCount = policy->schedulerCount;
            for(i = 0; i < maxCount; ++i) {
                policy->schedulers[i]->fcts.stop(policy->schedulers[i], newRl);
            }

            maxCount = policy->allocatorCount;
            for(i = 0; i < maxCount; ++i) {
                policy->allocators[i]->fcts.stop(policy->allocators[i], newRl);
            }

            // We could release our GUID here but not really required

            maxCount = policy->guidProviderCount;
            for(i = 0; i < maxCount; ++i) {
                policy->guidProviders[i]->fcts.stop(policy->guidProviders[i], newRl);
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
                policy->workers[i]->fcts.stop(policy->workers[i], RL_SHUTDOWN);
            }

            maxCount = policy->commApiCount;
            for(i = 0; i < maxCount; i++) {
                policy->commApis[i]->fcts.stop(policy->commApis[i], RL_SHUTDOWN);
            }

            maxCount = policy->schedulerCount;
            for(i = 0; i < maxCount; ++i) {
                policy->schedulers[i]->fcts.stop(policy->schedulers[i], RL_SHUTDOWN);
            }

            maxCount = policy->allocatorCount;
            for(i = 0; i < maxCount; ++i) {
                policy->allocators[i]->fcts.stop(policy->allocators[i], RL_SHUTDOWN);
            }

            maxCount = policy->guidProviderCount;
            for(i = 0; i < maxCount; ++i) {
                policy->guidProviders[i]->fcts.stop(policy->guidProviders[i], RL_SHUTDOWN);
            }
            break;
        }
        default:
            ASSERT("Unknown runlevel in stop function");
    }
}
#endif

void xePolicyDomainDestruct(ocrPolicyDomain_t * policy) {
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

static u8 xeProcessCeRequest(ocrPolicyDomain_t *self, ocrPolicyMsg_t **msg) {
    u8 returnCode = 0;
    u32 type = ((*msg)->type & PD_MSG_TYPE_ONLY);
    ocrPolicyDomainXe_t *rself = (ocrPolicyDomainXe_t*)self;
    ASSERT((*msg)->type & PD_MSG_REQUEST);
    if ((*msg)->type & PD_MSG_REQ_RESPONSE) {
        ocrMsgHandle_t *handle = NULL;
        returnCode = self->fcts.sendMessage(self, self->parentLocation, (*msg),
                                            &handle, (TWOWAY_MSG_PROP | PERSIST_MSG_PROP));
        if (returnCode == 0) {
            ASSERT(handle && handle->msg);
            ASSERT(handle->msg == *msg); // This is what we passed in
            RESULT_ASSERT(self->fcts.waitMessage(self, &handle), ==, 0);
            ASSERT(handle->response);
            // Check if the message was a proper response and came from the right place
            //ASSERT(handle->response->srcLocation == self->parentLocation);
            ASSERT(handle->response->destLocation == self->myLocation);
            if((handle->response->type & PD_MSG_TYPE_ONLY) != type) {
                // Special case: shutdown in progress, cancel this message
                // The handle is destroyed by the caller for this case
                if(!rself->shutdownInitiated)
                    ocrShutdown();
                else {
                    // We destroy the handle here (which frees the buffer)
                    handle->destruct(handle);
                }
                return OCR_ECANCELED;
            }
            ASSERT((handle->response->type & PD_MSG_TYPE_ONLY) == type);
            if(handle->response != *msg) {
                // We need to copy things back into *msg
                // TODO: FIXME when issue #68 is fully implemented by checking
                // sizes
                // We use the marshalling function to "copy" this message
                u64 baseSize = 0, marshalledSize = 0;
                ocrPolicyMsgGetMsgSize(handle->response, &baseSize, &marshalledSize, 0);
                // For now, it must fit in a single message
                ASSERT(baseSize + marshalledSize <= sizeof(ocrPolicyMsg_t));
                ocrPolicyMsgMarshallMsg(handle->response, baseSize, (u8*)*msg, MARSHALL_DUPLICATE);
            }
            handle->destruct(handle);
        }
    } else {
        returnCode = self->fcts.sendMessage(self, self->parentLocation, (*msg), NULL, 0);
    }
    return returnCode;
}

u8 xePolicyDomainProcessMessage(ocrPolicyDomain_t *self, ocrPolicyMsg_t *msg, u8 isBlocking) {

    u8 returnCode = 0;

    ASSERT((msg->type & PD_MSG_REQUEST) && (!(msg->type & PD_MSG_RESPONSE)));

    DPRINTF(DEBUG_LVL_VVERB, "Going to process message of type 0x%lx\n",
            (msg->type & PD_MSG_TYPE_ONLY));
    switch(msg->type & PD_MSG_TYPE_ONLY) {
    // First type of messages: things that we offload completely to the CE
    case PD_MSG_DB_CREATE: case PD_MSG_DB_DESTROY:
    case PD_MSG_DB_ACQUIRE: case PD_MSG_DB_RELEASE: case PD_MSG_DB_FREE:
    case PD_MSG_MEM_ALLOC: case PD_MSG_MEM_UNALLOC:
    case PD_MSG_WORK_CREATE: case PD_MSG_WORK_DESTROY:
    case PD_MSG_EDTTEMP_CREATE: case PD_MSG_EDTTEMP_DESTROY:
    case PD_MSG_EVT_CREATE: case PD_MSG_EVT_DESTROY: case PD_MSG_EVT_GET:
    case PD_MSG_GUID_CREATE: case PD_MSG_GUID_INFO: case PD_MSG_GUID_DESTROY:
    case PD_MSG_COMM_TAKE:
    case PD_MSG_DEP_ADD: case PD_MSG_DEP_REGSIGNALER: case PD_MSG_DEP_REGWAITER:
    case PD_MSG_DEP_SATISFY: {
        START_PROFILE(pd_xe_OffloadtoCE);
        if((msg->type & PD_MSG_TYPE_ONLY) == PD_MSG_WORK_CREATE) {
            START_PROFILE(pd_xe_resolveTemp);
#define PD_MSG msg
#define PD_TYPE PD_MSG_WORK_CREATE
            if((s32)(PD_MSG_FIELD_IO(paramc)) < 0) {
                localDeguidify(self, &(PD_MSG_FIELD_I(templateGuid)));
                ocrTaskTemplate_t *template = PD_MSG_FIELD_I(templateGuid).metaDataPtr;
                PD_MSG_FIELD_IO(paramc) = template->paramc;
            }
#undef PD_MSG
#undef PD_TYPE
            EXIT_PROFILE;
        }

        DPRINTF(DEBUG_LVL_VVERB, "Offloading message of type 0x%x to CE\n",
                msg->type & PD_MSG_TYPE_ONLY);
        returnCode = xeProcessCeRequest(self, &msg);

        if(((msg->type & PD_MSG_TYPE_ONLY) == PD_MSG_COMM_TAKE) && (returnCode == 0)) {
            START_PROFILE(pd_xe_Take);
#define PD_MSG msg
#define PD_TYPE PD_MSG_COMM_TAKE
            if (PD_MSG_FIELD_IO(guidCount) > 0) {
                DPRINTF(DEBUG_LVL_VVERB, "Received EDT with GUID 0x%lx (@ 0x%lx)\n",
                        PD_MSG_FIELD_IO(guids[0].guid), &(PD_MSG_FIELD_IO(guids[0].guid)));
                localDeguidify(self, (PD_MSG_FIELD_IO(guids)));
                DPRINTF(DEBUG_LVL_VVERB, "Received EDT (0x%lx; 0x%lx)\n",
                        (u64)self->myLocation, (PD_MSG_FIELD_IO(guids))->guid,
                        (PD_MSG_FIELD_IO(guids))->metaDataPtr);
                // For now, we return the execute function for EDTs
                PD_MSG_FIELD_IO(extra) = (u64)(self->taskFactories[0]->fcts.execute);
            }
#undef PD_MSG
#undef PD_TYPE
            EXIT_PROFILE;
        }
        EXIT_PROFILE;
        break;
    }

    // Messages are not handled at all
    case PD_MSG_COMM_GIVE: // Give should only be triggered from the CE
    case PD_MSG_WORK_EXECUTE: case PD_MSG_DEP_UNREGSIGNALER:
    case PD_MSG_DEP_UNREGWAITER: case PD_MSG_SAL_PRINT:
    case PD_MSG_SAL_READ: case PD_MSG_SAL_WRITE:
    case PD_MSG_MGT_REGISTER: case PD_MSG_MGT_UNREGISTER:
    case PD_MSG_SAL_TERMINATE:
    case PD_MSG_GUID_METADATA_CLONE: case PD_MSG_MGT_MONITOR_PROGRESS:
    {
        DPRINTF(DEBUG_LVL_WARN, "XE PD does not handle call of type 0x%x\n",
                (u32)(msg->type & PD_MSG_TYPE_ONLY));
        ASSERT(0);
        returnCode = OCR_ENOTSUP;
        break;
    }

    // Messages handled locally
    case PD_MSG_DEP_DYNADD: {
        START_PROFILE(pd_xe_DepDynAdd);
        ocrTask_t *curTask = NULL;
        getCurrentEnv(NULL, NULL, &curTask, NULL);
#define PD_MSG msg
#define PD_TYPE PD_MSG_DEP_DYNADD
        // Check to make sure that the EDT is only doing this to
        // itself
        // Also, this should only happen when there is an actual EDT
        ASSERT(curTask &&
               curTask->guid == PD_MSG_FIELD_I(edt.guid));

        DPRINTF(DEBUG_LVL_VVERB, "DEP_DYNADD req/resp for GUID 0x%lx\n",
                PD_MSG_FIELD_I(db.guid));
        ASSERT(curTask->fctId == self->taskFactories[0]->factoryId);
        PD_MSG_FIELD_O(returnDetail) = self->taskFactories[0]->fcts.notifyDbAcquire(curTask, PD_MSG_FIELD_I(db));
#undef PD_MSG
#undef PD_TYPE
        EXIT_PROFILE;
        break;
    }

    case PD_MSG_DEP_DYNREMOVE: {
        START_PROFILE(pd_xe_DepDynRemove);
        ocrTask_t *curTask = NULL;
        getCurrentEnv(NULL, NULL, &curTask, NULL);
#define PD_MSG msg
#define PD_TYPE PD_MSG_DEP_DYNREMOVE
        // Check to make sure that the EDT is only doing this to
        // itself
        // Also, this should only happen when there is an actual EDT
        ASSERT(curTask &&
            curTask->guid == PD_MSG_FIELD_I(edt.guid));
        DPRINTF(DEBUG_LVL_VVERB, "DEP_DYNREMOVE req/resp for GUID 0x%lx\n",
                PD_MSG_FIELD_I(db.guid));
        ASSERT(curTask->fctId == self->taskFactories[0]->factoryId);
        PD_MSG_FIELD_O(returnDetail) = self->taskFactories[0]->fcts.notifyDbRelease(curTask, PD_MSG_FIELD_I(db));
#undef PD_MSG
#undef PD_TYPE
        break;
    }
#if 0
    case PD_MSG_MGT_SHUTDOWN: {
        START_PROFILE(pd_xe_Shutdown);
        ocrPolicyDomainXe_t *rself = (ocrPolicyDomainXe_t*)self;
        rself->shutdownInitiated = 1;
        self->fcts.stop(self);
        if(msg->srcLocation == self->myLocation) {
            DPRINTF(DEBUG_LVL_VVERB, "MGT_SHUTDOWN initiation from 0x%lx\n",
                    msg->srcLocation);
            returnCode = xeProcessCeRequest(self, &msg);
            // HACK: We force the return code to be 0 here
            // because otherwise something will complain
            returnCode = 0;
        } else {
            //FIXME: We never exercise this code path.
            //Keeping it for now until we fix the shutdown protocol
            //Bug #134
            ASSERT(0);
            DPRINTF(DEBUG_LVL_VVERB, "MGT_SHUTDOWN(slave) from 0x%lx\n",
                    msg->srcLocation);
            // Send the message back saying that
            // we did the shutdown
            msg->destLocation = msg->srcLocation;
            msg->srcLocation = self->myLocation;
            msg->type &= ~PD_MSG_REQUEST;
            msg->type |= PD_MSG_RESPONSE;
            RESULT_ASSERT(self->fcts.sendMessage(self, msg->destLocation, msg, NULL, 0),
                          ==, 0);
            returnCode = 0;
        }
        EXIT_PROFILE;
        break;
    }
    case PD_MSG_MGT_FINISH: {
        START_PROFILE(pd_xe_Finish);
        self->fcts.finish(self);
        EXIT_PROFILE;
        break;
    }
#endif
    default: {
        DPRINTF(DEBUG_LVL_WARN, "Unknown message type 0x%x\n", (u32)(msg->type & PD_MSG_TYPE_ONLY));
        ASSERT(0);
    }
    }; // End of giant switch

    return returnCode;
}

u8 xePdSendMessage(ocrPolicyDomain_t* self, ocrLocation_t target, ocrPolicyMsg_t *message,
                   ocrMsgHandle_t **handle, u32 properties) {
    ocrPolicyDomainXe_t *rself = (ocrPolicyDomainXe_t*)self;
    ASSERT(target == self->parentLocation);
    // Set the header of this message
    message->srcLocation  = self->myLocation;
    message->destLocation = self->parentLocation;
    u8 returnCode = self->commApis[0]->fcts.sendMessage(self->commApis[0], target, message, handle, properties);
    if (returnCode == 0) return 0;
    switch (returnCode) {
    case OCR_ECANCELED:
        // Our outgoing message was cancelled and shutdown is assumed
        // TODO: later this will be expanded to include failures
        // We destruct the handle for the first message in case it was partially used
        if(*handle)
            (*handle)->destruct(*handle);
        //FIXME: OCR_ECANCELED shouldn't be inferred generally as shutdown
        //but rather handled on a per message basis. Once we have a proper
        //shutdown protocol this should go away.
        //Bug #134
        // We do not do a shutdown if we already have a shutdown initiated
        if(returnCode==OCR_ECANCELED && !rself->shutdownInitiated)
            ocrShutdown();
        break;
    case OCR_EBUSY:
        if(*handle)
            (*handle)->destruct(*handle);
        ocrMsgHandle_t *tempHandle = NULL;
        RESULT_ASSERT(self->fcts.waitMessage(self, &tempHandle), ==, 0);
        ASSERT(tempHandle && tempHandle->response);
        ocrPolicyMsg_t * newMsg = tempHandle->response;
        ASSERT(newMsg->srcLocation == self->parentLocation);
        ASSERT(newMsg->destLocation == self->myLocation);
        RESULT_ASSERT(self->fcts.processMessage(self, newMsg, true), ==, 0);
        tempHandle->destruct(tempHandle);
        break;
    default:
        ASSERT(0);
    }
    return returnCode;
}

u8 xePdPollMessage(ocrPolicyDomain_t *self, ocrMsgHandle_t **handle) {
    return self->commApis[0]->fcts.pollMessage(self->commApis[0], handle);
}

u8 xePdWaitMessage(ocrPolicyDomain_t *self,  ocrMsgHandle_t **handle) {
    return self->commApis[0]->fcts.waitMessage(self->commApis[0], handle);
}

void* xePdMalloc(ocrPolicyDomain_t *self, u64 size) {
    START_PROFILE(pd_xe_pdMalloc);
    void *ptr;
    PD_MSG_STACK(msg);
    ocrPolicyMsg_t* pmsg = &msg;
    getCurrentEnv(NULL, NULL, NULL, &msg);
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_MEM_ALLOC
    msg.type = PD_MSG_MEM_ALLOC  | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
    PD_MSG_FIELD_I(type) = DB_MEMTYPE;
    PD_MSG_FIELD_I(size) = size;
    ASSERT(self->workerCount == 1);              // Assert this XE has exactly one worker.
    u8 msgResult = xeProcessCeRequest(self, &pmsg);
    ASSERT (msgResult == 0);   // TODO: Are there error cases I need to handle?  How?
    ptr = PD_MSG_FIELD_O(ptr);
#undef PD_TYPE
#undef PD_MSG
    RETURN_PROFILE(ptr);
}

void xePdFree(ocrPolicyDomain_t *self, void* addr) {
    START_PROFILE(pd_xe_pdFree);
    PD_MSG_STACK(msg);
    ocrPolicyMsg_t *pmsg = &msg;
    getCurrentEnv(NULL, NULL, NULL, &msg);
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_MEM_UNALLOC
    msg.type = PD_MSG_MEM_UNALLOC | PD_MSG_REQUEST;
    PD_MSG_FIELD_I(ptr) = addr;
    PD_MSG_FIELD_I(type) = DB_MEMTYPE;
    // TODO: Things are missing. Brian's new way to free things should fix this!
    PD_MSG_FIELD_I(properties) = 0;
    u8 msgResult = xeProcessCeRequest(self, &pmsg);
    ASSERT(msgResult == 0);
#undef PD_MSG
#undef PD_TYPE
    RETURN_PROFILE();
}

ocrPolicyDomain_t * newPolicyDomainXe(ocrPolicyDomainFactory_t * factory,
#ifdef OCR_ENABLE_STATISTICS
                                      ocrStats_t *statsObject,
#endif
                                      ocrCost_t *costFunction, ocrParamList_t *perInstance) {

    ocrPolicyDomainXe_t * derived = (ocrPolicyDomainXe_t *) runtimeChunkAlloc(sizeof(ocrPolicyDomainXe_t), PERSISTENT_CHUNK);
    ocrPolicyDomain_t * base = (ocrPolicyDomain_t *) derived;

    ASSERT(base);
#ifdef OCR_ENABLE_STATISTICS
    factory->initialize(factory, base, statsObject, costFunction, perInstance);
#else
    factory->initialize(factory, base, costFunction, perInstance);
#endif
    derived->packedArgsLocation = NULL;

    return base;
}

void initializePolicyDomainXe(ocrPolicyDomainFactory_t * factory, ocrPolicyDomain_t* self,
#ifdef OCR_ENABLE_STATISTICS
                              ocrStats_t *statsObject,
#endif
                              ocrCost_t *costFunction, ocrParamList_t *perInstance) {
#ifdef OCR_ENABLE_STATISTICS
    self->statsObject = statsObject;
#endif

    initializePolicyDomainOcr(factory, self, perInstance);
}

static void destructPolicyDomainFactoryXe(ocrPolicyDomainFactory_t * factory) {
    runtimeChunkFree((u64)factory, NULL);
}

ocrPolicyDomainFactory_t * newPolicyDomainFactoryXe(ocrParamList_t *perType) {
    ocrPolicyDomainFactory_t* base = (ocrPolicyDomainFactory_t*) runtimeChunkAlloc(sizeof(ocrPolicyDomainFactoryXe_t), NONPERSISTENT_CHUNK);

    ASSERT(base); // Check allocation

#ifdef OCR_ENABLE_STATISTICS
    base->instantiate = FUNC_ADDR(ocrPolicyDomain_t*(*)(ocrPolicyDomainFactory_t*,ocrCost_t*,
                                  ocrParamList_t*), newPolicyDomainXe);
#endif

    base->instantiate = &newPolicyDomainXe;
    base->initialize = &initializePolicyDomainXe;
    base->destruct = &destructPolicyDomainFactoryXe;

    base->policyDomainFcts.switchRunlevel = FUNC_ADDR(u8 (*)(ocrPolicyDomain_t*, ocrRunlevel_t, u32), xePdSwitchRunlevel);
    base->policyDomainFcts.destruct = FUNC_ADDR(void(*)(ocrPolicyDomain_t*), xePolicyDomainDestruct);
    base->policyDomainFcts.processMessage = FUNC_ADDR(u8(*)(ocrPolicyDomain_t*,ocrPolicyMsg_t*,u8), xePolicyDomainProcessMessage);
    base->policyDomainFcts.sendMessage = FUNC_ADDR(u8(*)(ocrPolicyDomain_t*, ocrLocation_t, ocrPolicyMsg_t*, ocrMsgHandle_t**, u32),
                                         xePdSendMessage);
    base->policyDomainFcts.pollMessage = FUNC_ADDR(u8(*)(ocrPolicyDomain_t*, ocrMsgHandle_t**), xePdPollMessage);
    base->policyDomainFcts.waitMessage = FUNC_ADDR(u8(*)(ocrPolicyDomain_t*, ocrMsgHandle_t**), xePdWaitMessage);

    base->policyDomainFcts.pdMalloc = FUNC_ADDR(void*(*)(ocrPolicyDomain_t*,u64), xePdMalloc);
    base->policyDomainFcts.pdFree = FUNC_ADDR(void(*)(ocrPolicyDomain_t*,void*), xePdFree);
    return base;
}

#endif /* ENABLE_POLICY_DOMAIN_XE */
