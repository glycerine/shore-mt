/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: thread2.c,v 1.21 1997/04/21 20:31:23 bolo Exp $
 */
#include <iostream.h>
#include <strstream.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <memory.h>

#include <w.h>
#include <w_statistics.h>
#include <sthread.h>
#include <sthread_stats.h>

#ifndef IO_DIR
#define	IO_DIR	"/var/tmp"
#endif

extern "C" char *optarg; 

class io_thread_t : public sthread_t {
public:
	io_thread_t(int i, char *bf);

protected:
	virtual void run();

private:
	int		idx;
	char	*buf;
};


io_thread_t **ioThread;

int	NumIOs = 100;
int	NumThreads = 5;
int	BufSize = 1024;
int	vec_size = 0;		// i/o vector slots for an i/o operation
bool	local_io = false;
bool	fastpath_io = false;

char	*io_dir = IO_DIR;

/* build an i/o vector for an I/O operation, to test write buffer. */
int make_vec(char *buf, int size, int vec_size, 
		sthread_t::iovec *vec, const int iovec_max)
{
	int	slot = 0;

	if (vec_size == 0)
		vec_size = size;

	while (size > vec_size && slot < iovec_max-1) {
		vec[slot].iov_len = vec_size;
		vec[slot].iov_base = buf;
		buf += vec_size;
		size -= vec_size;
		slot++;
	}
	if (size) {
		vec[slot].iov_len = size;
		vec[slot].iov_base = buf;
		slot++;
	}

	return slot;
}



io_thread_t::io_thread_t(int i, char *bf)
: idx(i),
  buf(bf),
  sthread_t(t_regular)
{
	char buf[40];
	ostrstream s(buf, sizeof(buf));

	s << "io[" << idx << "]" << ends;
	rename(buf);
}


void io_thread_t::run()
{
	cout << name() << ": started" << endl;

	char fname[40];
	ostrstream f(fname, sizeof(fname));

	f.form("%s/sthread.%d.%d", io_dir, getpid(), idx);
	f << ends;
    
	int fd;
	w_rc_t rc;
	int flags = OPEN_RDWR | OPEN_SYNC | OPEN_CREATE;
	if (local_io)
		flags |= OPEN_LOCAL;
	else if (fastpath_io)
		flags |= OPEN_FAST;

	rc = sthread_t::open(fname, flags, 0666, fd);
	if (rc) {
		cerr << "open:" << endl << rc << endl;
		W_COERCE(rc);
	}

	int i; 
	for (i = 0; i < NumIOs; i++)  {
		iovec	vec[iovec_max];	/*XXX magic number */
		int	cnt;
		for (register j = 0; j < BufSize; j++)
			buf[j] = (unsigned char) i;

		cnt = make_vec(buf, BufSize, vec_size, vec, iovec_max);

		rc = sthread_t::writev(fd, vec, cnt);
		if (rc) {
			cerr << "write:" << endl << rc << endl;
			W_COERCE(rc);
		}
	}

	cout << name() << ": finished writing" << endl;

	off_t pos;
	if (rc = sthread_t::lseek(fd, 0, SEEK_SET, pos))  {
		cerr << "lseek:" << endl << rc << endl;
		W_COERCE(rc);
	}
    
	cout << name() << ": finished seeking" << endl;
	for (i = 0; i < NumIOs; i++) {
		if (rc = sthread_t::read(fd, buf, BufSize))  {
			cerr << "read:" << endl << rc << endl;
			W_COERCE(rc);
		}
		for (register j = 0; j < BufSize; j++) {
			if ((unsigned char)buf[j] != (unsigned char)i) {
				cout << name() << ": read bad data";
				cout.form(" (page %d  expected %d got %d\n",
					  i, i, buf[j]);
				W_FATAL(fcINTERNAL);
			}
		}
	}
	cout << name() << ": finished reading" << endl;
	
	W_COERCE( sthread_t::fsync(fd) );

	W_COERCE( sthread_t::close(fd) );

	// cleanup after ourself
	(void) ::unlink(fname);
}

main(int argc, char **argv)
{
    int c;
    int errors = 0;

    while ((c = getopt(argc, argv, "i:b:t:d:lfv:")) != EOF) {
	   switch (c) {
	   case 'i':
		   NumIOs = atoi(optarg);
		   break;
	   case 'b':
		   BufSize = atoi(optarg);
		   break;
	   case 't':
		   NumThreads = atoi(optarg);
		   break;
	   case 'd':
		   io_dir = optarg;
		   break;
	   case 'l':
		   local_io = true;
		   break;
	   case 'f':
		   fastpath_io = true;
		   break;
	   case 'v':
		   vec_size = atoi(optarg);
		   break;
	   default:
		   errors++;
		   break;
	   }
    }
    if (errors) {
	   cerr << "usage: "
		   << argv[0]
		   << " [-i num_ios]"
		   << " [-b bufsize]"
		   << " [-t num_threads]"
		   << " [-d directory]"
		   << " [-v vectors]" 	   
		   << " [-l]"	   
		   << endl;	   

	   return 1;
    }

    char *buf = (char*) sthread_t::set_bufsize(BufSize * NumThreads);
    assert(buf);

    ioThread = new io_thread_t *[NumThreads];
    assert(ioThread);

    cout << "Using "
	    << (local_io ? "local" : (fastpath_io ? "fastpath" : "diskrw"))
	    << " io." << endl;

    int i;
    for (i = 0; i < NumThreads; i++)  {
	ioThread[i] = new io_thread_t(i, buf + i*BufSize);
	w_assert1(ioThread[i]);
	W_COERCE(ioThread[i]->fork());
    }
    for (i = 0; i < NumThreads; i++)  {
	W_COERCE( ioThread[i]->wait() );
	delete ioThread[i];
    }
    delete [] ioThread;
    
    cout << SthreadStats << endl;

    (void) sthread_t::set_bufsize(0);

    return 0;
}

