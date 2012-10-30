/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: w_shmem.cc,v 1.14 1997/06/15 02:03:16 solomon Exp $
 */

#define W_SOURCE
#include <sys/types.h>
#include <w_base.h>

#ifdef __GNUC__
#pragma implementation
#endif

#include <w_shmem.h>
#ifdef PURIFY
#include <memory.h>
#include <purify.h>
#endif

w_rc_t
w_shmem_t::create(
    uint4_t	sz, 
    mode_t	mode)
{
    W_IGNORE( destroy() );
    _size = sz;

    if ((_id = shmget(IPC_PRIVATE, int(_size), IPC_CREAT|mode)) == -1)  {
	return RC(fcOS);
    }

    if ((_base = (char*)shmat(_id, 0, 0)) 
	== (char*)-1) {
	_base = 0;
	return RC(fcOS);
    }

    return RCOK;
}

w_rc_t
w_shmem_t::destroy()
{
    if (_base)  {
	W_DO( detach() );
	shmctl(_id, IPC_RMID, 0);
	_base = 0;
    }

    return RCOK;
}

w_rc_t
w_shmem_t::set(
    uid_t	uid, 
    gid_t	gid,
    mode_t 	mode)
{
    shmid_ds buf;
#ifdef PURIFY
	// even though the only parts of buf
	// that shmctl looks at are uid, gid, mode,
	// we clear the buf to avoid purify's UMC reports
	if(purify_is_running()) {
		memset(&buf, '\0', sizeof(buf));
	}
#endif
    if(uid)  buf.shm_perm.uid = uid;
    if(gid)  buf.shm_perm.gid = gid;
    if(mode) buf.shm_perm.mode = mode;

    if (shmctl(_id, IPC_SET, &buf) == -1) {
	return RC(fcOS); // error
    }
    _size = buf.shm_segsz;
    return RCOK;
}

w_rc_t
w_shmem_t::attach(int i)
{
    _id = i;

    if ((_base = (char*) shmat(_id, 0, 0)) == (char*)-1) {
	return RC(fcOS);
    }

    shmid_ds buf;
    if (shmctl(_id, IPC_STAT, &buf) == -1) {
	return RC(fcOS);
    }
    _size = buf.shm_segsz;

    return RCOK;
}

w_rc_t
w_shmem_t::detach()
{
    if((_base != NULL) && (shmdt(_base) < 0)) {
	return RC(fcOS);
    }
    return RCOK;
}

