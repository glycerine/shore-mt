/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/common/shmem.cc,v 1.8 1997/06/15 02:36:09 solomon Exp $
 */
#define SHMEM_C

#ifdef __GNUG__
#pragma implementation
#endif

#include <stdlib.h>
#include <unistd.h>
#include <iostream.h>
#include <assert.h>
#include <debug.h>
#include <sys/types.h>
#include "shmem.h"

int shmem_t::create(
	int 		sz, 
	unsigned int 	align,
	mode_t		mode
)
{
#ifdef domalloc
    int		tempsz = sz + align - 1;
#endif
    char 	*ataddr;

    destroy();
    _size = sz;

#ifdef domalloc
    if(align <= 0) return 1; // error

    _mbase = (char *)malloc(tempsz);
    ataddr = (char *)((((unsigned int)(_mbase + align -1))/align)*align);
#else
    ataddr = 0;
#endif

    if ((_id = shmget(IPC_PRIVATE, _size, IPC_CREAT|mode)) == -1)  {
	perror("shmget");
#ifdef domalloc
	free (_mbase);
#endif
	return 1; // error
    }

    if ((_base = (char*)shmat(_id, ataddr, 0)) 
	== (char*)-1) {
	perror("shmat");
#ifdef domalloc
	free (_mbase);
#endif
	_base = 0;
	return 1; // error
    }
#ifdef domalloc
    if(_base != ataddr) {
	destroy();
	return 1; // error
    }
#endif
    return 0; // success
}

void shmem_t::destroy()
{

    if (_base)  {
	(void) detach();
#ifdef domalloc
	assert(_mbase != 0);
	free (_mbase);
	_mbase = 0;
#endif
	shmctl(_id, IPC_RMID, 0);
	_base = 0;
    }
}

int shmem_t::set(uid_t	uid, gid_t gid, mode_t mode)
{
    shmid_ds buf;
    if(uid)  buf.shm_perm.uid = uid;
    if(gid)  buf.shm_perm.gid = gid;
    if(mode) buf.shm_perm.mode = mode;

    if (shmctl(_id, IPC_SET, &buf) == -1) {
	perror("shmctl");
	return 1; // error
    }
    _size = buf.shm_segsz;
    return 0; // ok
}

void shmem_t::attach(int i)
{
    _id = i;

    if ((_base = (char*) shmat(_id, 0, 0)) == (char*)-1) {
	perror("shmat");
	W_FATAL(fcOS);
    }

    shmid_ds buf;
    if (shmctl(_id, IPC_STAT, &buf) == -1) {
	perror("shmctl");
	W_FATAL(fcOS);
    }
    _size = buf.shm_segsz;
}

int shmem_t::detach()
{
    if((_base != NULL) && (shmdt(_base) < 0)) {
	perror("shmdt");
	return 1; // error
    }
    return 0;  // success
}

