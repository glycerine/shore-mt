/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#include <iostream.h>
#include <strstream.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>
#include <memory.h>

#include <w.h>
#include <w_statistics.h>
#include <sthread.h>
#include <sthread_stats.h>

extern "C" char *optarg;

ostream &operator<<(ostream &o, const struct disk_geometry &dg)
{
	W_FORM(o)("   %8d  %10d   %10d %10d %10d",
		  dg.blocks, dg.block_size,
		  dg.cylinders, dg.tracks, dg.sectors);
	return o;
}

void print_dg(const char *fname, const struct disk_geometry &dg)
{
	W_FORM(cout)("%-20s", fname);
	cout << dg << endl;
}

void label_dg()
{
	W_FORM(cout)("%-20s   %8s  %10s   %10s %10s %10s\n",
		     "Device", "Blocks", "BlockSize",
		     "Cylinders", "Tracks", "Sectors");
}

w_rc_t test_diskinfo(const char *fname, bool stamp, struct disk_geometry *all)
	
{
	int fd;
	w_rc_t rc;
	int flags = sthread_t::OPEN_RDONLY | sthread_t::OPEN_LOCAL;
	struct	disk_geometry	dg;

	rc = sthread_t::open(fname, flags, 0666, fd);
	if (rc) {
		cerr << "Can't open '" << fname << "':" << endl << rc << endl;
		return rc;
	}

	rc = sthread_t::fgetgeometry(fd, dg);
	if (rc) {
		cerr << "Can't get disk geometry:" << endl << rc << endl;
		return rc;
	}

	if (stamp)
		*all = dg;
	else {
		all->cylinders += dg.cylinders;
		all->blocks += dg.blocks;
	}

	print_dg(fname, dg);

	W_COERCE( sthread_t::close(fd) );

	return RCOK;
}

main(int argc, char **argv)
{
	int i;

	if (argc < 2) {
		cerr << "usage: " << argv[0] << " device ..." << endl;
		return 1;
	}

	char *buf = (char*) sthread_t::set_bufsize(1024*1024);
	if (!buf)
		W_FATAL(fcOUTOFMEMORY);

	struct disk_geometry all;
	memset(&all, '\0', sizeof(all));

	label_dg();
	for (i = 1; i < argc; i++)
		W_IGNORE(test_diskinfo(argv[i], i == 1, &all));

	if (argc > 2)
		print_dg("All", all);

	(void) sthread_t::set_bufsize(0);

	return 0;
}

