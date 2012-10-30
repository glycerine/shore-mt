/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 * $Header: /p/shore/shore_cvs/src/common/shmem.h,v 1.8 1995/04/24 19:28:38 zwilling Exp $
 */
#ifndef SHMEM_H
#define SHMEM_H

#ifdef __GNUG__
#pragma interface
#endif

#if defined(Mips) || defined(Sparc) || defined(I860)
#define	SHM_PROTO_KLUDGE
#endif
#if defined(SHM_PROTO_KLUDGE)
#   define shmget __shmget
#   define shmat __shmat
#   define shmctl __shmctl
#   define shmdt __shmdt
#if defined(Mips)
#   include <mips/pte.h>
#   include <sys/param.h>
#endif
#   include <sys/types.h>
#   include <sys/ipc.h>
#   include <sys/shm.h>
#   undef shmget
#   undef shmat
#   undef shmctl
#   undef shmdt
    extern "C" {
        int shmget(key_t, int, int);
        char *shmat(int, void*, int);
        int shmctl(int, int, struct shmid_ds *);
        int shmdt(const void*);
    }
#else
#   include <sys/types.h>
#   include <sys/ipc.h>
#   include <sys/shm.h>
#endif /* SHM_PROTO_KLUDGE */

#if defined(I860)
#	ifdef __cplusplus
	extern "C" {
#	endif
		extern int getpagesize();
#	ifdef __cplusplus
	};
#	endif
#endif

class shmem_t {
public:
    shmem_t() : _base(0), _id(0) 	{};
    ~shmem_t()			{};

    id()		{ return _id; }
    size()		{ return _size; }
    char* base()	{ return _base; }

    int	 create(int sz,	// return value of 0 == OK, 1 == FAILURE
		unsigned int 	align=SHMLBA,
		mode_t		mode = 0644
	);
    void destroy();

    void attach(int id);
    int  detach();
    int	 set(uid_t uid=0, gid_t gid=0, mode_t mode=0);
private:
    char* _mbase;	// malloced address
    char* _base;	// attached address
    int _id;
    int _size;
};

#endif /*SHMEM_H*/
