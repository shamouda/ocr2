#include <ocr-types.h>
#include <ocr-config.h>

#ifdef ENABLE_BUILDER_ONLY

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include <libelf.h>
#include <gelf.h>

typedef struct _func_entry {
    u64 offset;
    char *func_name;
} func_entry;

#define MAXFNS (2048)
#define BASE_ADDRESS (0x0)

// The below holds all functions in the app
func_entry func_list[MAXFNS];
u32 func_list_sz = 0;
u64 dram_offset = 0;
u64 end_marker = 0;

#define END1 "_end"
#define END2 "_end_local"

static void check_for_end_marker (char *symname, u64 symaddr) {
    if(!strcmp(symname, END1) || !strcmp(symname, END2)) {
        end_marker = symaddr;
        end_marker += (symaddr%0x40)?(0x40-symaddr%0x40):(0); // Align to 64-byte boundary
    }
}

/* Below code adapted from libelf tutorial example */
static u32 extract_functions_internal (const char *str, func_entry *func_list) {
    Elf *e;
    Elf_Kind ek;
    Elf_Scn *scn;
    Elf_Data *edata;
    u32 fd, i, symbol_count;
    GElf_Sym sym;
    GElf_Shdr shdr;
    u32 func_count = 0;

    if(elf_version(EV_CURRENT) == EV_NONE) {
        printf("Error initializing ELF: %s\n", elf_errmsg(-1));
        return -1;
    }

    if ((fd = open(str, O_RDONLY, 0)) < 0) {
        printf("Unable to open %s\n", str);
        return -1;
    }

    if ((e = elf_begin(fd, ELF_C_READ, NULL)) == NULL) {
        printf("elf_begin failed: %s\n", elf_errmsg(-1));
    }

    ek = elf_kind(e);
    if(ek != ELF_K_ELF) {
        printf("not an ELF object");
    } else {
        scn = NULL;
        edata = NULL;
        while((scn = elf_nextscn(e, scn)) != NULL) {
            gelf_getshdr(scn, &shdr);
            if(shdr.sh_type == SHT_SYMTAB) {
                edata = elf_getdata(scn, edata);
                symbol_count = shdr.sh_size / shdr.sh_entsize;
                for(i = 0; i < symbol_count; i++) {
                    gelf_getsym(edata, i, &sym);
                    if(ELF32_ST_TYPE(sym.st_info) != STT_FUNC) {
                        check_for_end_marker(elf_strptr(e, shdr.sh_link, sym.st_name), sym.st_value);
                        continue;
                    }
                    if(sym.st_size == 0) continue;

                    func_list[func_count].offset = sym.st_value;
                    func_list[func_count++].func_name = strdup(elf_strptr(e, shdr.sh_link, sym.st_name));

                    if(func_count >= MAXFNS) {
                        printf("Func limit (%"PRId32") reached, please increase MAXFNS & rebuild\n", MAXFNS);
                        raise(SIGABRT);
                    }
                    //                    printf("%08x %08x\t%s\n", sym.st_value, sym.st_size, elf_strptr(e, shdr.sh_link, sym.st_name));
                }
            }
        }
    }

    elf_end(e);
    close(fd);
    return func_count;
}

static void free_func_names(func_entry *func_lists, u32 func_list_size) {
    u32 i;
    for(i = 0; i < func_list_size; i++)
        free(func_lists[i].func_name);
}

void extract_functions (const char *str) {
    func_list_sz = extract_functions_internal (str, func_list);
}

void free_functions (void) {
    free_func_names(func_list, func_list_sz);
}

char* find_function(u64 address, func_entry *func_lists, u32 func_list_size) {
    u32 i;

    for(i = 0; i<func_list_size; i++)
        if(func_lists[i].offset == address)
            return func_lists[i].func_name;

    return NULL;
}

u64 find_function_address (char *fname, func_entry *func_lists, u32 func_list_size) {
    u32 i;

    for(i = 0; i<func_list_size; i++)
        if(!strcmp(func_lists[i].func_name, fname)) {
            return func_lists[i].offset;
        }

    printf("Warning: %s not found in app, continuing...\n", fname);
    return 0;
}

void *getAddress (char *fname) {
    return (void *)(dram_offset + find_function_address(fname, func_list, func_list_sz));
}

#endif
