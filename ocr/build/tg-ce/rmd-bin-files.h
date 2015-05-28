#ifndef __RMD_BIN_FILES_H__
#define __RMD_BIN_FILES_H__

#define PD_SWITCH_RL_OFFSET              (0x08)  // Offset of switchRunlevel in ocrPolicyDomainFcts_t
#define XE_PDARGS_OFFSET                 (0x168) // sizeof(ocrPolicyDomain_t)

extern int rmd_load_struct_binaries(void);
extern int rmd_call_policy_domain(void);

#endif
