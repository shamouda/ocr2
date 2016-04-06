#include "ocr-config.h"
#ifdef ENABLE_NEWLIB_SCAFFOLD
#define _GNU_SOURCE
#include <features.h>
#include <stdint.h>
#include <errno.h>
typedef long _off_t;
#include <fcntl.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/_types.h>
#include <sys/reent.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <signal.h>

#include <ocr.h>
#include <extensions/ocr-legacy.h>

#define _NOARGS void

//
// Note: This file needs to be compiled with the platform (Linux) includes
//       instead of newlib's since we need to use the platform syscalls.
//       However, this leads to mismatches in newlib vs. platform types,
//       structures, and flags which we need to translate back and forth.

#define ocr_assert(expr)  // for now no realization

void __assert_func(void)
{
}

///////////////////////////////////////////////////////////////////////////////
// Newlib to/from Linux platform data translations
//
// Convert from Linux struct stat to newlib's
// Newlib system type sizes have been matched to Linux x86_64 std.
//
struct newlib_stat
{
  dev_t        nst_dev;
  ino_t        nst_ino;
  mode_t    nst_mode;
  nlink_t    nst_nlink;
  uid_t        nst_uid;
  gid_t        nst_gid;
  dev_t        nst_rdev;
  off_t        nst_size;
  time_t    nst_atime;
  long        nst_spare1;
  time_t    nst_mtime;
  long        nst_spare2;
  time_t    nst_ctime;
  long        nst_spare3;
  long        nst_blksize;
  long        nst_blocks;
  long        nst_spare4[2];
};
//
// Do a manual field by field copy from the system stat to the newlib one
//
void stat_copy( struct stat * st, struct stat * lst )
{
    struct newlib_stat *nst = (struct newlib_stat *) st;

    nst->nst_dev = lst->st_dev;
    nst->nst_ino = lst->st_ino;
    nst->nst_mode = lst->st_mode;
    nst->nst_nlink = lst->st_nlink;
    nst->nst_uid = lst->st_uid;
    nst->nst_gid = lst->st_gid;
    nst->nst_rdev = lst->st_rdev;
    nst->nst_size = lst->st_size;
    nst->nst_atime = lst->st_atime;
    nst->nst_mtime = lst->st_mtime;
    nst->nst_ctime = lst->st_ctime;
    nst->nst_blksize = lst->st_blksize;
    nst->nst_blocks = lst->st_blocks;
}

//
// translate newlib open flags to Linux
//
#define NL_O_RDONLY 0
#define NL_O_WRONLY 1
#define NL_O_RDWR   2
#define NL_O_APPEND 0x0008
#define NL_O_CREAT  0x0200
#define NL_O_TRUNC  0x0400
#define NL_O_EXCL   0x0800

#define NL_O_FLAGS  (NL_O_WRONLY|NL_O_RDWR|NL_O_APPEND| \
                     NL_O_CREAT|NL_O_TRUNC|NL_O_EXCL)

static int nlflags_to_linux( int nlflags )
{
    ocr_assert( nlflags & ~NL_O_FLAGS == 0 );

    int lflags = 0;
    if( nlflags & NL_O_WRONLY ) lflags |= O_WRONLY;
    if( nlflags & NL_O_RDWR   ) lflags |= O_RDWR;
    if( nlflags & NL_O_APPEND ) lflags |= O_APPEND;
    if( nlflags & NL_O_CREAT  ) lflags |= O_CREAT;
    if( nlflags & NL_O_TRUNC  ) lflags |= O_TRUNC;
    if( nlflags & NL_O_EXCL   ) lflags |= O_EXCL;

    return lflags;
}


///////////////////////////////////////////////////////////////////////////////
// System call shim definition

struct _ocr_reent ocrreent;
struct _reent *_impure_ptr = (struct _reent *)&ocrreent;

static inline int isNull( ocrGuid_t g ) { return ocrGuidIsNull(g); }

//
// We tag guids as types
//
enum ocr_guid_type {
    GUID_NONE,
    GUID_APP,
    GUID_FD,
    GUID_MEMORY,
    GUID_CONTEXT
};
//
// Access methods
//
#define setGuidType( v, t ) (((uint64_t)(t) << 60) | (v))
#define getGuidType( v )    ((uint64_t)(t) >> 60)
#define getGuidValue( h )   ((h.guid) & (((uint64_t)1 << 60) - 1))
#define isGuidType( h, t )  (getGuidType(h) == (t))

//
// Track GUIDs we hand out so that we can free our resources
// Static allocate to avoid using malloc (could use mmap I guess ...)
//
#define NGUIDS    16
#define INIT_GUID 0xFFFFFFFFFFFFFFFF
#define FREE_GUID 0xFFFFFFFFFFFFFFF0


static
struct guidmap {
    ocrGuid_t guid;
    uint64_t addr;
    uint64_t len;
} Guids[NGUIDS] = {
    { .addr = 0, .len = 0 }
};

static int
add_guid( ocrGuid_t guid, uint64_t addr, uint64_t len )
{
    u32 i;
    //
    // init on first use
    //
    if( (Guids[0].addr == 0) && (Guids[0].len == 0) ) {
        for( i = 0 ; i < NGUIDS ; i++ )
            Guids[i].guid.guid = FREE_GUID;
    }
    for( i = 0 ; i < NGUIDS ; i++ ) {
        if( Guids[i].guid.guid == FREE_GUID ) {
            Guids[i].guid.guid = guid.guid;
            Guids[i].addr = addr;
            Guids[i].len = len;
            return 0;
        }
    }
    return 1;
}

///////////////////////////////////////////////////////////////////////////////
// OCR Init/Shutdown methods
//
void ocrInit( ocrGuid_t *legacyContext, ocrConfig_t * ocrConfig )
{
    ocr_assert( legacyContext != NULL );
    ocr_assert( ocrConfig != NULL );

    ocrLegacyInit(legacyContext, ocrConfig);
}

u8 ocrFinalize(ocrGuid_t legacyContext)
{
    ocr_assert( isGuidType( legacyContext, GUID_CONTEXT ) );

    return 0;
}

u8 ocrLegacyContextAssociateMemory( ocrGuid_t legacyContext,
                                    ocrGuid_t db)
{
    ocr_assert( isGuidType( legacyContext, GUID_CONTEXT ) );
    ocr_assert( isGuidType( db, GUID_MEMORY ) );

    return 0;
}

u8 ocrLegacyContextRemoveMemory( ocrGuid_t legacyContext,
                                 ocrGuid_t db)
{
    ocr_assert( isGuidType( legacyContext, GUID_CONTEXT ) );
    ocr_assert( isGuidType( db, GUID_MEMORY ) );

    return 0;
}

///////////////////////////////////////////////////////////////////////////////
// OCR SAL methods
//

u8 ocrUSalOpen( ocrGuid_t legacyContext, ocrGuid_t* handle,
                const char * file, s32 flags, s32 mode )
{
    ocr_assert( isGuidType( legacyContext, GUID_CONTEXT ) );
    ocr_assert( handle != NULL );
    ocr_assert( file != NULL );

    flags = nlflags_to_linux( flags );

    int retval = open( file, flags, mode );

    if( retval >= 0 ) {
        ((ocrGuid_t *)handle)->guid = setGuidType( retval, GUID_FD );
        add_guid( *handle, 0, 0 );
    }

    return retval < 0;
}

u8 ocrUSalClose(ocrGuid_t legacyContext, ocrGuid_t handle)
{
    if( isNull(handle) )
        return -1;
    ocr_assert( isGuidType( legacyContext, GUID_CONTEXT ) );
    ocr_assert( isGuidType( handle, GUID_FD ) );

    return close( getGuidValue(handle) ) < 0;
}

u8 ocrUSalRead( ocrGuid_t legacyContext, ocrGuid_t handle,
                s32 *readCount, char * ptr, s32 len)
{
    if( isNull(handle) )
        return -1;
    ocr_assert( isGuidType( legacyContext, GUID_CONTEXT ) );
    ocr_assert( isGuidType( handle, GUID_FD ) );

    int nread = read( getGuidValue(handle), (void *) ptr, (size_t) len );

    if( nread >= 0 )
        *readCount = nread;

    return nread < 0;
}

u8 ocrUSalWrite( ocrGuid_t legacyContext, ocrGuid_t handle,
                 s32 *wroteCount, const char * ptr, s32 len)
{
    if( isNull(handle) )
        return -1;
    ocr_assert( isGuidType( legacyContext, GUID_CONTEXT ) );
    ocr_assert( isGuidType( handle, GUID_FD ) );

    int nwritten = write( getGuidValue(handle), (const void *) ptr, (size_t) len );

    if( nwritten >= 0 )
        *wroteCount = nwritten;

    return nwritten < 0;
}

//
// debugging hook
//
void do_catch()
{
//    ocr_errno = 0;
}

u8 ocrUSalChown(ocrGuid_t legacyContext, const char* path, uid_t owner, gid_t group)
{
    ocr_assert( isGuidType( legacyContext, GUID_CONTEXT ) );
    return (u8) (chown( path, owner, group ) == -1);
}

//
// mode_t mappings seem to match between newlib and Linux. So no translation
// between newlib and Linux needed.
//
u8 ocrUSalChmod(ocrGuid_t legacyContext, const char* path, mode_t mode)
{
    ocr_assert( isGuidType( legacyContext, GUID_CONTEXT ) );
    return (u8) (chmod( path, mode ) == -1);
}
u8 ocrUSalChdir(ocrGuid_t legacyContext, const char* path)
{
    ocr_assert( isGuidType( legacyContext, GUID_CONTEXT ) );
    return (u8) (chdir( path ) == -1);
}
u8 ocrIsAtty (s32 file)
{
    do_catch(); return 0;
}
s64 ocrReadlink (ocrGuid_t legacyContext, const char *path, char *buf, size_t bufsize)
{
    ocr_assert( isGuidType( legacyContext, GUID_CONTEXT ) );
    return readlink( path, buf, bufsize );
}
u8 ocrUSalSymlink(ocrGuid_t legacyContext, const char* path1, const char* path2)
{
    ocr_assert( isGuidType( legacyContext, GUID_CONTEXT ) );
    return (u8) (symlink( path1, path2 ) == -1);
}
u8 ocrUSalLink(ocrGuid_t legacyContext, const char* existing, const char* new)
{
    ocr_assert( isGuidType( legacyContext, GUID_CONTEXT ) );
    return (u8) (link( existing, new ) == -1);
}
u8 ocrUSalUnlink(ocrGuid_t legacyContext, const char* name)
{
    ocr_assert( isGuidType( legacyContext, GUID_CONTEXT ) );
    return (u8) (unlink( name ) == -1);
}
//
// This file is compiled with the Linux host includes and it's struct stat
// is a different size and order than newlib's. So we do a manual copy-over
// to avoid trashing our stack, etc.
//
u8 ocrUSalFStat(ocrGuid_t legacyContext, ocrGuid_t handle, struct stat* st)
{
    if( isNull(handle) )
        return -1;
    ocr_assert( isGuidType( legacyContext, GUID_CONTEXT ) );
    ocr_assert( isGuidType( handle, GUID_FD ) );
    ocr_assert( st != NULL );
    struct stat lst;
    int ret = fstat( getGuidValue(handle), & lst );

    if( ret == 0 )
        stat_copy( st, & lst );

    return (u8) (ret == -1);
}
u8 ocrUSalStat(ocrGuid_t legacyContext, const char* file, struct stat* st)
{
    ocr_assert( isGuidType( legacyContext, GUID_CONTEXT ) );
    return (u8) (stat( file, st ) == -1);
}
s64 ocrUSalLseek(ocrGuid_t legacyContext, ocrGuid_t handle, s64 offset, s32 whence)
{
    if( isNull(handle) )
        return -1;
    ocr_assert( isGuidType( legacyContext, GUID_CONTEXT ) );
    ocr_assert( isGuidType( handle, GUID_FD ) );
    return (s64) lseek( getGuidValue(handle), (off_t) offset, (int) whence );
}

u8 ocrFork (_NOARGS)
{
    return -1;
}
u8 ocrEvecve (char *name, char **argv, char **env)
{
    return -1;
}
u8 ocrGetPID (_NOARGS)
{
    return (u8) (getpid( ) == -1);
}
u8 ocrKill (s32 pid, s32 sig)
{
    return (u8) (kill( pid, sig ) == -1);
}
u8 ocrGetTimeofDay (struct timeval  *ptimeval, void *ptimezone)
{
    return (u8) (gettimeofday( ptimeval, ptimezone ) == -1);
}

#endif
