/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_EXTENSION_LIB

#include "debug.h"
#include "machine-description/ocr-machine.h"
#include "extensions/ocr-lib.h"
#include "ocr-policy-domain.h"
#include "ocr-types.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#warning Experimental OCR library support enabled

enum {
    OPT_NONE, OPT_CONFIG, OPT_VERSION, OPT_HELP
};

// Helper methods
static struct options {
    char *flag;
    char *env_flag;
    s32 option;
    char *help;
} ocrOptionDesc[] = {
    {
        "cfg", "OCR_CONFIG", OPT_CONFIG, "-ocr:cfg <file> : the OCR runtime configuration file to use."
    },
    {
        "version", "", OPT_VERSION, "-ocr:version : print OCR version"
    },
    {
        "help", "", OPT_HELP, "-ocr:help : print this message"
    },
    {
        NULL, NULL, 0, NULL
    }
};

static void printHelp(void) {
    struct options *p;

    fprintf(stderr, "Usage: program [<OCR options>] [<program options>]\n");
    fprintf(stderr, "OCR options:\n");

    for (p = ocrOptionDesc; p->flag; ++p)
        if (p->help)
            fprintf(stderr, "    %s, env: %s\n", p->help, p->env_flag);

    fprintf(stderr, "\n");
    fprintf(stderr, "https://github.com/01org/ocr\n");
}

static void printVersion(void) {
    fprintf(stderr, "Open Community Runtime (OCR) %s%s\n", "0.9", "");
}

static void setIniFile(ocrConfig_t * ocrConfig, const char * value) {
    struct stat st;
    if (stat(value, &st) != 0) {
        fprintf(stderr, "ERROR: cannot find runtime configuration file: %s\n", value);
        exit(1);
    }
    ocrConfig->iniFile = value;
}

static inline void checkNextArgExists(s32 i, s32 argc, char * option) {
    if (i == argc) {
        fprintf(stderr, "ERROR: No argument for OCR option %s\n", option);
        exit(1);
    }
}

static void checkOcrOption(ocrConfig_t * ocrConfig) {
    if (ocrConfig->iniFile == NULL) {
        fprintf(stderr, "ERROR: no runtime configuration file provided\n");
        exit(1);
    }
}

static void readFromEnv(ocrConfig_t * ocrConfig) {
    // Go over OCR options description and check
    // if some of the env variables are set.
    struct options  *p;
    for (p = ocrOptionDesc; p->flag; ++p) {
        char * opt = getenv(p->env_flag);
        // If variable defined and has value
        if ((opt != NULL) && (strcmp(opt, "") != 0)) {
            switch (p->option) {
            case OPT_CONFIG:
                setIniFile(ocrConfig, opt);
                break;
            }
        }
    }
}

static s32 readFromArgs(s32 argc, const char* argv[], ocrConfig_t * ocrConfig) {
    // Override any env variable with command line option
    s32 cur = 1;
    s32 userArgs = argc;
    char * ocrOptPrefix = "-ocr:";
    s32 ocrOptPrefixLg = strlen(ocrOptPrefix);
    while(cur < argc) {
        const char * arg = argv[cur];
        if (strncmp(ocrOptPrefix, arg, ocrOptPrefixLg) == 0) {
            // This is an OCR option
            const char * ocrArg = arg+ocrOptPrefixLg;
            if (strcmp("cfg", ocrArg) == 0) {
                checkNextArgExists(cur, argc, "cfg");
                setIniFile(ocrConfig, argv[cur+1]);
                argv[cur] = NULL;
                argv[cur+1] = NULL;
                cur++; // skip param
                userArgs-=2;
            } else if (strcmp("version", ocrArg) == 0) {
                printVersion();
                exit(0);
                break;
            } else if (strcmp("help", ocrArg) == 0) {
                printHelp();
                exit(0);
                break;
            }
        }
        cur++;
    }
    return userArgs;
}

static void ocrConfigInit(ocrConfig_t * ocrConfig) {
    ocrConfig->userArgc = 0;
    ocrConfig->userArgv = NULL;
    ocrConfig->iniFile = NULL;
}

// Public methods

// All these externs are in ocr-driver.c.
extern void bringUpRuntime(const char *inifile);
extern void freeUpRuntime(void);
extern void stopAllPD(ocrPolicyDomain_t *pd);

/**!
 * Setup and starts the OCR runtime.
 * @param ocrConfig Data-structure containing runtime options
 */
void ocrInit(ocrConfig_t * ocrConfig) {
    const char * iniFile = ocrConfig->iniFile;
    // Configuration file is missing, please check OCR_CONFIG env var or -ocr:cfg option
    ASSERT(iniFile != NULL);
    bringUpRuntime(iniFile);
}

void ocrParseArgs(s32 argc, const char* argv[], ocrConfig_t * ocrConfig) {

    // Zero-ed the ocrConfig
    ocrConfigInit(ocrConfig);

    // First retrieve options from environment variables
    readFromEnv(ocrConfig);

    // Override any env variable with command line args
    s32 userArgs = readFromArgs(argc, argv, ocrConfig);

    // Check for mandatory options
    checkOcrOption(ocrConfig);

    // Pack argument list
    s32 cur = 0;
    s32 head = 0;
    while(cur < argc) {
        if(argv[cur] != NULL) {
            if (cur == head) {
                head++;
            } else {
                argv[head] = argv[cur];
                argv[cur] = NULL;
                head++;
            }
        }
        cur++;
    }
    ocrConfig->userArgc = userArgs;
    ocrConfig->userArgv = (char **) argv;
}

// This function is for x86 for now. We may need to make this a function
// pointer for other platforms
u8 ocrFinalize() {
    ocrPolicyDomain_t *pd = NULL;
    ocrWorker_t *worker = NULL;
    getCurrentEnv(&pd, &worker, NULL, NULL);
    // We start the current worker. After it starts, it will loop
    // until ocrShutdown is called which will cause the entire PD
    // to stop (including this worker). The currently executing
    // worker then fallthrough from start to finish.
    worker->fcts.start(worker, pd);
    u8 returnCode = pd->shutdownCode;
    // NOTE: finish blocks until stop has completed
    pd->fcts.finish(pd);
    pd->fcts.destruct(pd);
    freeUpRuntime();
// #ifdef OCR_ENABLE_STATISTICS
//     ocrStatsProcessDestruct(&GfakeProcess);
//     GocrFilterAggregator->destruct(GocrFilterAggregator);
// #endif
   return returnCode;
}

#endif /* ENABLE_EXTENSION_LIB */
