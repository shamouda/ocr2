#ifndef __RMD_BIN_FILES_H__
#define __RMD_BIN_FILES_H__

#define PDBEGIN_OFFSET              (0x08)
#define PDSTART_OFFSET              (0x10)   /* Ugly hard-coding, but the alternative is */
#define XE_PDARGS_OFFSET            (0x158)  /* to pollute rmdkrnl with OCR structs.     */

extern int rmd_load_struct_binaries(void);
extern int rmd_call_policy_domain(void);

#endif
