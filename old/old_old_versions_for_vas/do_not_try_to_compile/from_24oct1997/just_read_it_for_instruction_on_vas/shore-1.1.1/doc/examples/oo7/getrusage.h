#include <sys/time.h>
#include <sys/resource.h>

#ifdef _HPUX_SOURCE

/*$Header: /p/shore/shore_cvs/src/oo7/getrusage.h,v 1.3 1994/03/22 21:56:31 nhall Exp $*/
//
// getrusage.h
//
// Getrusge system call for HP-UX.
//



#include <sys/syscall.h>

extern "C" int syscall(int number, ...);

#define getrusage(a,b)	syscall(SYS_GETRUSAGE, a, b)

#else
extern "C" int getrusage(int, struct rusage *);
#endif
