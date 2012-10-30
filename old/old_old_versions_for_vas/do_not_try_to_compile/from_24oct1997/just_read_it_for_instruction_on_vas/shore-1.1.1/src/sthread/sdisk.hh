/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
    /* XXX actually disk geometry isn't needed ... just the
       blocksize and size of a raw partition */
struct disk_geometry {
	off_t	blocks;		// # of blocks on device
	size_t	block_size;	// # bytes in each block
	
	/* this may be an approximation */
	size_t	cylinders;
	size_t	tracks;
	size_t	sectors;
};

extern	w_rc_t	sdisk_getgeometry(int fd, struct disk_geometry &dg);

#include <fcntl.h>	/* Unix O_ modes */
#include <sys/uio.h>	/* struct iovec */
