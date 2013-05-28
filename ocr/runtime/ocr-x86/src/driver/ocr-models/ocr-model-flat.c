#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "ocr-runtime.h"
#include "ocr-config.h"
#include "ocr-guid.h"
#include "ocr-macros.h"

static ocr_model_policy_t * defaultOcrModelPolicy(size_t nb_policy_domains, size_t nb_schedulers,
                                           size_t nb_workers, size_t nb_comp_targets, size_t nb_workpiles);

void ocrModelInitFlat(char * mdFile) {
    u32 nbHardThreads = ocr_config_default_nb_hardware_threads;
    if (mdFile != NULL) {
        //TODO need a file stat to check
        setMachineDescriptionFromPDL(mdFile);
        MachineDescription * md = getMachineDescription();
        if (md == NULL) {
            // Something went wrong when reading the machine description file
            ocr_abort();
        } else {
            nbHardThreads = MachineDescription_getNumHardwareThreads(md);
            //  gHackTotalMemSize = MachineDescription_getDramSize(md);
        }
    }

    // This is the default policy
    // TODO this should be declared in the default policy model
    size_t nb_policy_domain = 1;
    size_t nb_workers_per_policy_domain = nbHardThreads;
    size_t nb_workpiles_per_policy_domain = nbHardThreads;
    size_t nb_comp_target_per_policy_domain = nbHardThreads;
    size_t nb_schedulers_per_policy_domain = 1;

    ocr_model_policy_t * policy_model = defaultOcrModelPolicy(nb_policy_domain,
                                                              nb_schedulers_per_policy_domain, nb_workers_per_policy_domain,
                                                              nb_comp_target_per_policy_domain, nb_workpiles_per_policy_domain);

    //TODO LIMITATION for now support only one policy
    n_root_policy_nodes = nb_policy_domain;
    root_policies = instantiateModel(policy_model);

    root_policies[0]->n_successors = 0;
    root_policies[0]->successors = NULL;
    root_policies[0]->n_predecessors = 0;
    root_policies[0]->predecessors = NULL;

    master_worker = root_policies[0]->workers[0];
    root_policies[0]->start(root_policies[0]);

}
/**
 * Default policy has one scheduler and a configurable
 * number of workers, comp-targets and workpiles
 */
static ocr_model_policy_t * defaultOcrModelPolicy(size_t nb_policy_domains, size_t nb_schedulers,
                                           size_t nb_workers, size_t nb_comp_targets, size_t nb_workpiles) {

    // Default policy
    ocr_model_policy_t * defaultPolicy =
        (ocr_model_policy_t *) checked_malloc(defaultPolicy, sizeof(ocr_model_policy_t));
    defaultPolicy->model.kind = ocr_policy_default_kind;
    defaultPolicy->model.nb_instances = nb_policy_domains;
    defaultPolicy->model.per_type_configuration = NULL;
    defaultPolicy->model.per_instance_configuration = NULL;

    defaultPolicy->nb_scheduler_types = 1;
    defaultPolicy->nb_worker_types = 1;
    defaultPolicy->nb_comp_target_types = 1;
    defaultPolicy->nb_workpile_types = 1;
    defaultPolicy->numMemTypes = 1;
    defaultPolicy->numAllocTypes = 1;

    // Default allocator
    ocrAllocatorModel_t *defaultAllocator =
        (ocrAllocatorModel_t*)malloc(sizeof(ocrAllocatorModel_t));
    defaultAllocator->model.per_type_configuration = NULL;
    defaultAllocator->model.per_instance_configuration = NULL;
    defaultAllocator->model.kind = OCR_ALLOCATOR_DEFAULT;
    defaultAllocator->model.nb_instances = 1;
    defaultAllocator->sizeManaged = gHackTotalMemSize;

    defaultPolicy->allocators = defaultAllocator;

    size_t index_config = 0, n_all_schedulers = nb_schedulers*nb_policy_domains;

    void** scheduler_configurations = malloc(sizeof(scheduler_configuration*)*n_all_schedulers);
    for ( index_config = 0; index_config < n_all_schedulers; ++index_config ) {
        scheduler_configurations[index_config] = (scheduler_configuration*) malloc(sizeof(scheduler_configuration));
        scheduler_configuration* curr_config = (scheduler_configuration*)scheduler_configurations[index_config];
        curr_config->worker_id_begin = ( index_config / nb_schedulers ) * nb_workers;
        curr_config->worker_id_end = ( index_config / nb_schedulers ) * nb_workers + nb_workers - 1;
    }

    size_t n_all_workers = nb_workers*nb_policy_domains;

    index_config = 0;
    void** worker_configurations = malloc(sizeof(worker_configuration*)*n_all_workers );
    for ( index_config = 0; index_config < n_all_workers; ++index_config ) {
        worker_configurations[index_config] = (worker_configuration*) malloc(sizeof(worker_configuration));
        worker_configuration* curr_config = (worker_configuration*)worker_configurations[index_config];
        curr_config->worker_id = index_config;
    }

    defaultPolicy->schedulers = newModel( ocr_scheduler_default_kind, nb_schedulers, NULL, scheduler_configurations );
    defaultPolicy->compTargets  = newModel( ocr_comp_target_default_kind, nb_comp_targets, NULL, NULL );
    defaultPolicy->workpiles  = newModel( ocr_workpile_default_kind, nb_workpiles, NULL, NULL );
    defaultPolicy->memories   = newModel( OCR_MEMPLATFORM_DEFAULT, 1, NULL, NULL );
    defaultPolicy->workers    = newModel( ocr_worker_default_kind, nb_workers, NULL, worker_configurations);


    // Defines how ocr modules are bound together
    size_t nb_module_mappings = 5;
    ocr_module_mapping_t * defaultMapping =
        (ocr_module_mapping_t *) checked_malloc(defaultMapping, sizeof(ocr_module_mapping_t) * nb_module_mappings);
    // Note: this doesn't bind modules magically. You need to have a mapping function defined
    //       and set in the targeted implementation (see ocr_scheduler_hc implementation for reference).
    //       These just make sure the mapping functions you have defined are called
    defaultMapping[0] = build_ocr_module_mapping(MANY_TO_ONE_MAPPING, OCR_WORKPILE, OCR_SCHEDULER);
    defaultMapping[1] = build_ocr_module_mapping(ONE_TO_ONE_MAPPING, OCR_WORKER, OCR_COMP_TARGET);
    defaultMapping[2] = build_ocr_module_mapping(ONE_TO_MANY_MAPPING, OCR_SCHEDULER, OCR_WORKER);
    defaultMapping[3] = build_ocr_module_mapping(MANY_TO_ONE_MAPPING, OCR_MEMORY, OCR_ALLOCATOR);
    defaultMapping[4] = build_ocr_module_mapping(MANY_TO_ONE_MAPPING, OCR_SCHEDULER, OCR_POLICY);
    defaultPolicy->nb_mappings = nb_module_mappings;
    defaultPolicy->mappings = defaultMapping;

    return defaultPolicy;
}