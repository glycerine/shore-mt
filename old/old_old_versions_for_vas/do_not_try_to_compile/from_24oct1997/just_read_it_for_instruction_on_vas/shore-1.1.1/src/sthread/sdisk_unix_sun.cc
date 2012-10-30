/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#include <w.h>
#include <sdisk.hh>

#include <iostream.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <st_error.h>	/* XXX */

#if defined(SOLARIS2)
#include <sys/dkio.h>
#include <sys/vtoc.h>
#include <sys/param.h>
#define	HAVE_GEOMETRY
#endif

#if defined(SUNOS41)
// we should get dk_map from <sun/dkio.h> but its definition is not c++ friendly
struct dk_map {
	daddr_t dkl_cylno;      /* starting cylinder */
	daddr_t dkl_nblk;       /* number of 512-byte blocks */
};
#include <sun/dkio.h>
#include <sys/param.h>
#define	HAVE_GEOMETRY
#endif

#if !defined(SOLARIS2) && !defined(AIX41)
extern "C" int ioctl(int, int, ...);
#endif

w_rc_t	sdisk_getgeometry(int fd, struct disk_geometry &dg)
{
	w_rc_t	e;
	int	n;
	struct	stat	st;

	n = fstat(fd, &st);
	if (n == -1)
		return RC(fcOS);

	/* only raw disks have a geometry */
	if ((st.st_mode & S_IFMT) != S_IFCHR)
		return RC(stINVALIDIO);

#if defined(SOLARIS2)
	// use the size of the raw partition for the log
	struct dk_cinfo		cinfo;
	struct dk_allmap	allmap;
	struct vtoc		vtoc;
	
	n = ioctl(fd, DKIOCINFO, &cinfo);
	if (n == -1) {
		e = RC(fcOS);
		cerr << "ioctl(DKIOCINFO):" << endl << e << endl;
	        return e;
	}
	
	n = ioctl(fd, DKIOCGAPART, &allmap);
	if (n == -1) {
		e = RC(fcOS);
		cerr << "ioctl(DKIOCGAPART):" << endl << e << endl;
		return e;
	}

	n = ioctl(fd, DKIOCGVTOC, &vtoc);
	if (n == -1) {
		e = RC(fcOS);
		cerr << "ioctl(DKIOCGVTOC):" << endl << e << endl;
		return e;
	}
	
	dg.block_size = vtoc.v_sectorsz;
	dg.blocks = allmap.dka_map[cinfo.dki_partition].dkl_nblk;
#endif

#if defined(SUNOS41)
	struct dk_info	info;
	struct dk_map	map;

	n = ioctl(fd, DKIOCINFO, &info);
	if (n == -1) {
		e = RC(fcOS);
		cerr << "ioctl(DKIOCINFO):" << endl << e << endl;
		return e;
	}

	if (info.dki_ctype != DKC_SCSI_CCS) {
		e = RC(fcINTERNAL);
		cerr << "non-scsi drives not supported at user-level"
			<< endl << e << endl;
		return e;
	}
	
	n = ioctl(fd, DKIOCGPART, &map);
	if (n == -1) {
		e = RC(fcOS);
		cerr << "ioctl(DKIOCGAPART):" << endl << e << endl;
		return e;
	}
	
	dg.block_size = DEV_BSIZE;
	dg.blocks = map.dkl_nblk;
#endif

#if defined(SUNOS41) || defined(SOLARIS2) 
	struct dk_geom      geom;

	n = ioctl(fd, DKIOCGGEOM, &geom);
	if (n == -1) {
		e = RC(fcOS);
		cerr << "ioctl(DKIOCGGEOM):" << endl << e << endl;
		return e;
	}

	dg.sectors = geom.dkg_nsect;
	dg.tracks = geom.dkg_nhead;
	dg.cylinders = geom.dkg_ncyl;	// only valid for 'c' or 'd'
#endif

#ifdef HAVE_GEOMETRY
	/* No way to find #cylinders in a partition; calculate it by hand.
	   Assume (valid for most everything) that partitions are
	   cylinder aligned. */
	dg.cylinders = dg.blocks / (dg.sectors * dg.tracks);
#else
	e = RC(fcNOTIMPLEMENTED);
	cerr << "fgetgeometry(): only implemented on SunOS and Solaris"
		<< endl << e << endl;
	dg.cylinders = 0;
	dg.tracks = 0;
	dg.sectors = 0;
	dg.block_size = 0;
	dg.blocks = 0;
#endif
	return e;
}

