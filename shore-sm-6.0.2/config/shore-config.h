/* config/shore-config.h.  Generated by configure.  */
/* config/shore-config-h.in.  Generated from configure.ac by autoheader.  */

/* old debug code included */
/* #undef DEBUG */

/*if is gcc2.96*/
#if defined(__GNUC__) && __GNUC__==2
# if defined(__GNUC_MINOR__) && __GNUC_MINOR__==96
# define GCC_BROKEN_WARNINGS
#endif
#endif


/* hack for gcc3.4 and later */
/* #undef GCC_VER_34_WARNINGS */

/*if is gcc3.x, x < 4*/
#if defined(__GNUC__) && (__GNUC__==3)
# define GCC_VER_3_WARNINGS
#endif


/* Define to 1 if you have the <atomic.h> header file. */
/* #undef HAVE_ATOMIC_H */

/* Define to 1 if the system has the type `char_t'. */
/* #undef HAVE_CHAR_T */

/* Define to 1 if you have the `clock_gettime' function. */
/* #undef HAVE_CLOCK_GETTIME */

/* Define to 1 if you have the declaration of `MAP_ALIGN', and to 0 if you
   don't. */
#define HAVE_DECL_MAP_ALIGN 0

/* Define to 1 if you have the declaration of `MAP_ANON', and to 0 if you
   don't. */
#define HAVE_DECL_MAP_ANON 1

/* Define to 1 if you have the declaration of `MAP_ANONYMOUS', and to 0 if you
   don't. */
#define HAVE_DECL_MAP_ANONYMOUS 1

/* Define to 1 if you have the declaration of `MAP_FIXED', and to 0 if you
   don't. */
#define HAVE_DECL_MAP_FIXED 1

/* Define to 1 if you have the declaration of `MAP_NORESERVE', and to 0 if you
   don't. */
#define HAVE_DECL_MAP_NORESERVE 1

/* Define to 1 if you have the <dirent.h> header file, and it defines `DIR'.
   */
#define HAVE_DIRENT_H 1

/* Define to 1 if you don't have `vprintf' but do have `_doprnt.' */
/* #undef HAVE_DOPRNT */

/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 1

/* Define to 1 if you have the `getaddrinfo' function. */
#define HAVE_GETADDRINFO 1

/* Define to 1 if you have the `gethostbyname' function. */
#define HAVE_GETHOSTBYNAME 1

/* Define to 1 if you have the `gethostname' function. */
#define HAVE_GETHOSTNAME 1

/* Define to 1 if you have the `gethrtime' function. */
/* #undef HAVE_GETHRTIME */

/* Define to 1 if you have the `getopt' function. */
#define HAVE_GETOPT 1

/* Define to 1 if you have the `getpagesizes' function. */
/* #undef HAVE_GETPAGESIZES */

/* Define to 1 if you have the `gettimeofday' function. */
#define HAVE_GETTIMEOFDAY 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the `mtmalloc' library (-lmtmalloc). */
/* #undef HAVE_LIBMTMALLOC */

/* Define to 1 if you have the `rt' library (-lrt). */
#define HAVE_LIBRT 1

/* Define to 1 if the system has the type `long_t'. */
/* #undef HAVE_LONG_T */

/* Define to 1 if you have the `memalign' function. */
#define HAVE_MEMALIGN 1

/* Define to 1 if you have the `membar_enter' function. */
/* #undef HAVE_MEMBAR_ENTER */

/* Define to 1 if you have the `memcntl' function. */
/* #undef HAVE_MEMCNTL */

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the <ndir.h> header file, and it defines `DIR'. */
/* #undef HAVE_NDIR_H */

/* Define to 1 if you have the <netdb.h> header file. */
#define HAVE_NETDB_H 1

/* Define to 1 if you have the <netinet/in.h> header file. */
#define HAVE_NETINET_IN_H 1

/* Define to 1 if you have the `posix_memalign' function. */
#define HAVE_POSIX_MEMALIGN 1

/* Define to 1 if you have the `posix_spawn' function. */
#define HAVE_POSIX_SPAWN 1

/* have pthread_attr_getstack? */
#define HAVE_PTHREAD_ATTR_GETSTACK 1

/* have pthread_attr_getstacksize? */
#define HAVE_PTHREAD_ATTR_GETSTACKSIZE 1

/* have pthreads barriers? */
#define HAVE_PTHREAD_BARRIERS 1

/* Define to 1 if the system has the type `ptrdiff_t'. */
#define HAVE_PTRDIFF_T 1

/* Define to 1 if the system has the type `short_t'. */
/* #undef HAVE_SHORT_T */

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the `strerror' function. */
#define HAVE_STRERROR 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if `st_blksize' is member of `struct stat'. */
#define HAVE_STRUCT_STAT_ST_BLKSIZE 1

/* Define to 1 if you have the <sys/dir.h> header file, and it defines `DIR'.
   */
/* #undef HAVE_SYS_DIR_H */

/* Define to 1 if you have the <sys/ioctl.h> header file. */
#define HAVE_SYS_IOCTL_H 1

/* Define to 1 if you have the <sys/ndir.h> header file, and it defines `DIR'.
   */
/* #undef HAVE_SYS_NDIR_H */

/* Define to 1 if you have the <sys/param.h> header file. */
#define HAVE_SYS_PARAM_H 1

/* Define to 1 if you have the <sys/socket.h> header file. */
#define HAVE_SYS_SOCKET_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <time.h> header file. */
#define HAVE_TIME_H 1

/* Define to 1 if the system has the type `uchar_t'. */
/* #undef HAVE_UCHAR_T */

/* Define to 1 if the system has the type `ulong_t'. */
/* #undef HAVE_ULONG_T */

/* Define to 1 if you have the `uname' function. */
#define HAVE_UNAME 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if the system has the type `ushort_t'. */
/* #undef HAVE_USHORT_T */

/* Define to 1 if you have the <valgrind.h> header file. */
/* #undef HAVE_VALGRIND_H */

/* Define to 1 if you have the <valgrind/valgrind.h> header file. */
/* #undef HAVE_VALGRIND_VALGRIND_H */

/* Define to 1 if you have the `valloc' function. */
#define HAVE_VALLOC 1

/* Define to 1 if you have the `vprintf' function. */
#define HAVE_VPRINTF 1

/* operating system */
/* #undef HPUX8 */

/* huge page size */
#define HUGEPAGESIZE 2048

/* architecture */
/* #undef I386 */

/* architecture */
/* #undef I860 */

/* architecture */
/* #undef IA64 */

/* combo */
/* #undef IS_AMD64 */

/* amd64 linux */
#define IS_AMD64_LINUX 1

/* hp unix */
/* #undef IS_HPUX8 */

/* i386 linux */
/* #undef IS_I386_LINUX */

/* combo */
/* #undef IS_I386_SOLARIS2 */

/* ia64 linux */
/* #undef IS_IA64_LINUX */

/* linux */
#define IS_LINUX 1

/* hp unix */
/* #undef IS_SNAKE_HPUX8 */

/* is solaris */
/* #undef IS_SOLARIS2 */

/* is sparc solaris */
/* #undef IS_SPARC_SOLARIS2 */

/* combo */
/* #undef IS_SPARC_SUNOS41 */

/* operating system */
/* #undef IS_SUNOS41 */

/* combo */
/* #undef IS_X86_64 */

/* x86_64 linux */
#define IS_X86_64_LINUX 1

/* operating system */
#define Linux 

/* no debug code included */
/* #undef NDEBUG */

/* Name of package */
#define PACKAGE "shore-sm"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "nhall@cs.wisc.edu."

/* Define to the full name of this package. */
#define PACKAGE_NAME "shore-sm"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "shore-sm 6.0.2"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "shore-sm"

/* Define to the version of this package. */
#define PACKAGE_VERSION "6.0.2"

/* The size of a `pthread_t', as computed by sizeof. */
#define SIZEOF_PTHREAD_T 8

/* DORA-related code included */
/* #undef SM_DORA */

/* configured page size */
#define SM_PAGESIZE 8192

/* operating system */
/* #undef SOLARIS2 */

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* operating system */
/* #undef SUNOS41 */

/* architecture */
/* #undef Snake */

/* architecture */
/* #undef Sparc */

/* include tracing code? */
#define TRACECODE 1

/* enable-pthread-mutex ? */
#define USE_PTHREAD_MUTEX 1

/* Version number of package */
#define VERSION "6.0.2"

/* Define to 1 if your processor stores words with the most significant byte
   first (like Motorola and SPARC, unlike Intel and VAX). */
/* #undef WORDS_BIGENDIAN */

/* Use debug level 0, optimized */
#define W_DEBUG_LEVEL 3

/* track return-code checking */
#define W_DEBUG_RC 1

#if (defined(_LARGEFILE_SOURCE) && defined(_FILE_OFFSET_BITS)) || defined(ARCH_LP64)
#define LARGEFILE_AWARE
#endif


#if TRACECODE==1
#define W_TRACE
#endif


/*for in-memory str streams*/
#define W_USE_COMPAT_STRSTREAM


/* architecture */
#define amd64 1

/* Define to `int' if <sys/types.h> doesn't define. */
/* #undef gid_t */

/*turn off hpux and hp9000s800*/
#if defined(hpux) && defined(hp9000s800)
/* #undef hpux */
/* #undef hp9000s800 */
#endif


/* architecture - not always defined by compiler */
/* #undef i386 */

/* Define to `int' if <sys/types.h> does not define. */
/* #undef mode_t */

/* Define to `long' if <sys/types.h> does not define. */
/* #undef off_t */

/* Define to `int' if <sys/types.h> does not define. */
/* #undef pid_t */

/* Define to `unsigned' if <sys/types.h> does not define. */
/* #undef size_t */

/* Define to `int' if <sys/types.h> doesn't define. */
/* #undef uid_t */

/* architecture */
#define x86_64 
