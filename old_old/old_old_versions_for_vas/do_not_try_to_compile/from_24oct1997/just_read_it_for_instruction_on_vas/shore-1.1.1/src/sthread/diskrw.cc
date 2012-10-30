/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#define DISKRW_C

#include <copyright.h>
#include <stdio.h>
#include <iostream.h>
#include <fstream.h>
#include <w_statistics.h>
#include <strstream.h>
#include <w_signal.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/uio.h>

#include <unistd.h>

#include <string.h>
#include <sys/time.h>
#include <sys/resource.h>

#if	!defined(_SC_OPEN_MAX) && defined(RLIMIT_NOFILE)
extern "C"	int getrlimit(int, struct rlimit *);
#endif
#ifndef RLIMIT_NOFILE
extern "C" int getdtablesize();
#endif

#if !defined(AIX41) && !defined(SOLARIS2) && !defined(Linux)
extern "C" {
	extern int writev(int, const struct iovec *, int);
	extern int readv(int, const struct iovec *, int);
}
#endif

#include <w_base.h>
#include <w_shmem.h>

#define STATS ShmcStats

#include "spin.h"
#include "diskrw.h"
#include "st_error.h"
#include "diskstats.h"


enum {
	stOS = fcOS,
};

/* Only do prefetch on supported architectures */
#if defined(PREFETCH) && !(defined(SOLARIS2) || defined(SUNOS41))
#undef PREFETCH
#define PREFETCH_WARN
#endif
#if defined(PREFETCH)
extern "C" {
#if !defined(SOLARIS2) && !defined(AIX41)
	int ioctl(int, int, ...);
#endif
#if !defined(_SC_PAGESIZE)
	int getpagesize(); 	
#endif
}
#endif

/*
 *  Local variables
 *
 *	fname		: file name of device or unix file
 *	shmem_seg	: shared memory segment
 *	ppid		: parent (server) process id
 *	kill_siblings	: true if process should kill all siblings
 *			  when it dies (for the monitor process).
 *	fflags		: open flags for file
 *
 *  and, if DISKSTATSDIR is set:
 *	path		: file name in which to write disk stats
 *                        (determined by environment vbl DISKSTATSDIR and
 *                        file name in fname)
 *	statfd		: file descriptor for stats file
 *	u		: unix_stats structure
 *	s		: system call counts structure
 *	smode		: create-mode for stats file
 */
char* 		fname = "<noname-diskrw>";
w_shmem_t 	shmem_seg;
pid_t 		ppid;
bool 		kill_siblings = 0;
int		fflags = 0;
int 		statfd = -1;
const char *	path=0;
int 		smode = 0755;



/*********************************************************************
 *
 *  Function declarations
 *
 *********************************************************************/
void 		caught_signal(int sig);
void 		setup_signal();
void 		writestats(bool closeit=false);



/*********************************************************************
 *
 *  fatal(line)
 *
 *  Print fatal error message and exit.
 *
 *********************************************************************/
void
fatal(int line)
{
    W_FORM(cerr)("diskrw: pid=%d: %s: fatal error at line %d\n", 
	getpid(), fname, line);
    kill(ppid, SIGKILL);
    exit(-1);
}



/*********************************************************************
 *
 *  is_raw_device(devname)
 *
 *  Return true if "devname" is a raw device. False otherwise.
 *
 *********************************************************************/
bool
is_raw_device(const char* devname)
{
    bool ret = false;
    struct stat     statInfo;
    if (stat(devname, &statInfo) < 0) {
	cerr << "diskrw (" << fname
            << "): cannot stat \"" << devname << "\"" << endl;
    } else {
	// if it's a character device, its a raw disk
	ret = ((statInfo.st_mode & S_IFMT) == S_IFCHR);
    }
    return ret;
}

/*********************************************************************
 *
 *  get_disk_info(devname)
 *
 *  get disk block info for "devname"  -- only used for
 *  raw devices
 *
 *********************************************************************/

#ifdef PREFETCH
#include "sdisk.hh"

struct  diskinfo_t {
    long nblk;	    // # sectors total
    long blksz;     // sector size
    long nsect; // # sectors per track
    long tracksize; // in bytes
    long lastfulltrack; // in bytes
};

static	diskinfo_t D;
int	sys_pagesize;
int	tracksize_in_pages;

w_rc_t
get_disk_info(const char *devname, diskinfo_t &d)
{
	// FUNC(get_disk_info);
	int fr;
	fr  = ::open(devname, O_RDONLY, 0); // open for read only

	w_rc_t	e;
	struct	disk_geometry	dg;

	e = sdisk_get_geometry(fr, dg);
	if (e) {
		close(fr);
		return e;
	}

	d.nsect = dg.sectors;
	d.nblk = dg.blocks;
	d.blksz = dg.block_size;
	d.tracksize = d.nsect * d.blksz; 

	if (tracksize_in_pages > 0)
		d.tracksize = sys_pagesize * tracksize_in_pages;

	d.lastfulltrack = (d.nblk * d.blksz) / d.tracksize;

	/*
	   cerr << "raw device has " <<  d.nblk 
	   << " blocks of size " << d.blksz
	   << " nsect is "  << d.nsect
	   << " tracksize is "  << d.tracksize
	   << " lastfulltrack is "  << d.lastfulltrack
	   <<endl;
	   */

	::close(fr);

	return RCOK;
}
#endif


/*********************************************************************
 *
 *  struct wbufdatum_t
 *
 *  A request to write "len" bytes of memory at "ptr" to "offset"
 *  on disk.
 *
 *********************************************************************/
struct wbufdatum_t {
    off_t 			offset;	
    char* 			ptr;
    int 			len;

    NORET			wbufdatum_t()
    	: offset(0), ptr(0), len(0)	{};
    NORET			wbufdatum_t(off_t off, char* p, int l)
	: offset(off), ptr(p), len(l)   {};

};


/*********************************************************************
 * 
 *  class writebuf_t
 *
 *********************************************************************/
class writebuf_t {
public:
    NORET			writebuf_t() : offset(-1), sz(0)   {};
    NORET			~writebuf_t();

    int 			deposit(const wbufdatum_t& d, int f = 0);
    void 			withdraw_all(wbufdatum_t& d);
    void 			withdrawal_done();
protected:
    enum { 
	bufsz = 64 * 1024,
    };
    off_t 			offset;
    char 			buf[bufsz];
    int  			sz;
};




/*********************************************************************
 *
 *  writebuf_t::~writebuf_t()
 *
 *  Destructor. Check that bank is empty.
 *
 *********************************************************************/
NORET
writebuf_t::~writebuf_t()
{
    if (sz > 0)  {
	cerr << "writebuf_t::~writebuf_t  :- not empty" << endl;
    } if (sz < 0) {
	w_assert3(sz == -1);
	cerr << "writebuf_t::~writebuf_t  :- withdrawal not done"
	     << endl;
    }
}



/*********************************************************************
 *
 *  writebuf_t::withdraw_all(d)
 *
 *  Withdraw all deposits so far. 
 *
 *********************************************************************/
inline void
writebuf_t::withdraw_all(wbufdatum_t& d)  
{
    w_assert3(sz >= 0);
    d.offset = offset;
    d.ptr = buf;
    d.len = sz;
    sz = -1;
    offset = -1;
}




/*********************************************************************
 *
 *  writebuf_t::withdrawal_done()
 *
 *  Signal that all bytes withdrawn are spent (written to disk).
 *
 *********************************************************************/
inline void
writebuf_t::withdrawal_done()  
{ 
    w_assert3(sz == -1);
    sz = 0;
    offset = -1;
}



/*********************************************************************
 *
 *  writebuf_t::deposit(d, print_error)
 *
 *  Deposit the request "d" into writebuf. "Print_error" indicates
 *  whether to print out an error message if the operation fails
 *  (for debugging).
 *
 *********************************************************************/
int
writebuf_t::deposit(const wbufdatum_t& d, int print_error)
{
    /*
     *  Check if in middle of a withdrawal.
     */
    if (sz == -1) {
	if (print_error)  cerr << "diskrw (" << fname
            << "): withdrawal not done" << endl;
	return -1;	// withdrawal not done
    }

    /*
     *  Check if deposited bytes are not contiguous with 
     *  current holdings.
     */
    w_assert3(sz >= 0);
    if (offset >= 0 && offset + sz != d.offset) {
	if (print_error)  cerr << "diskrw (" << fname
            << "): discontinuity" << endl;
	return -1;	// discontinuity
    }

    /*
     *  Check if buffer will go bust with new deposits.
     */
    if (d.len > bufsz - sz)  {
	if (print_error)  cerr << "diskrw (" << fname
            << "): overflow" << endl;
	return -1; // overflow
    }

    /*
     *  If this is the first deposit (after a full withdrawal) ...
     */
    if (offset < 0)  {
	w_assert3(offset == -1);
	w_assert3(sz == 0);
	offset = d.offset;
    }

    /*
     *  Copy from d 
     */
    memcpy(buf + sz, d.ptr, d.len);
    sz += d.len;

    return 0;
}

#ifdef PREFETCH
/*********************************************************************
 * 
 *  class readbuf_t
 *
 *********************************************************************/
class readbuf_t : public writebuf_t {
public:
    void			discard(const wbufdatum_t& d); 
    bool 			read_track(const wbufdatum_t& d) ;
    NORET			readbuf_t(int _fd): writebuf_t(), 
				    track_num(-1), fd(_fd) {}

private:
    int				track_num; // track of contents
    int				fd;
};

bool
readbuf_t::read_track(const wbufdatum_t &d)
{
   w_assert3(D.tracksize <= this->bufsz);

   // return true if read succeeded, either by reading
   // from buf or from disk

   // does the request span tracks?  If so, return false
   // and don't disturb the buffer.
	
   int start = d.offset / D.tracksize;
   int end = (d.offset + d.len) / D.tracksize;
   if(start != end) return false; // spans tracks.

   // is it a legitimate track?
   if(start > D.lastfulltrack) return false; // bad track

   if(start != track_num) {
	// read the correct track
	errno = 0;
	int cc=0;
	off_t track_offset = start * D.tracksize;

	if (lseek(fd, track_offset, SEEK_SET) == -1) {
	    cerr.form("diskrw: %s: lseek(%ld): %s\n",
		      fname, (long) track_offset, strerror(errno));
	    fatal(__LINE__);
	}
	/*
	cerr << " reading track " << start 
		<< " for request: " << d.offset
		<< "," << d.len 
		<<endl;
	*/

	if ( (cc = ::read(fd, buf, D.tracksize)) != D.tracksize)   {
	    cerr.form("diskrw: %s: read(%d @ %ld)==%d: %s\n",
		  fname, D.tracksize, track_offset, cc,
		  errno ? strerror(errno) : "<no error>");
	    fatal(__LINE__);
	}
	track_num = start;
	offset = track_offset;
	sz = D.tracksize;

	s.reads++;
	s.bread += D.tracksize;
   } else {
	/*
	cerr << "SKIP READ: track " << start 
		<< " for request: " << d.offset
		<< "," << d.len 
		<< endl;
	*/
	s.skipreads++;
   }

   w_assert3(start == track_num);
   // copy the appropriate part out

   w_assert3(/* m.off */d.offset  >= offset);
   w_assert3((/* m.off */d.offset - offset)+d.len  <= sz);

   off_t buf_off = d.offset - offset;

   memcpy(d.ptr, buf + buf_off, d.len);

   return true;
}

void
readbuf_t::discard(const wbufdatum_t& d)
{
   int start = d.offset / D.tracksize;
   if(start != track_num) {
        int end = (d.offset + d.len) / D.tracksize;
	if(end != track_num) return;
   }
   // either starts or ends in this track
   // so discard the buffered info

   // NB: could just cache it here

   offset = -1;  
   sz = 0;
   track_num = -1;
   s.discards++;
}
#endif /*PREFETCH*/


/*********************************************************************
 *
 *  Local variable
 *
 *********************************************************************/
static writebuf_t* wbuf = 0;
#ifdef PREFETCH
static readbuf_t* rbuf = 0;
// a losing proposition -- the disk does all the buffering
// we need.
bool prefetch_on = false;
#endif /*PREFETCH*/


/*********************************************************************
 *
 *  flush_wbuf(fd)
 *
 *********************************************************************/
void
flush_wbuf(int fd)
{
    wbufdatum_t d;
    wbuf->withdraw_all(d);
    if (d.len)  {
	if (d.len < 0)  fatal(__LINE__);
	if (lseek(fd, d.offset, SEEK_SET) == -1) {
	    cerr.form("diskrw: %s: lseek(%ld): %s\n",
		      fname, (long) d.offset, strerror(errno));
	    fatal(__LINE__);
	}
	if (write(fd, d.ptr, d.len) != d.len)  {
	    cerr.form("diskrw: %s: write(%d): %s\n",
		      fname, d.len, strerror(errno));
	    fatal(__LINE__);
	}
	s.writes++;
	s.bwritten += d.len;
    }

    wbuf->withdrawal_done();
}



/*********************************************************************
 *
 *  main(argc, argv)
 *
 *  Usage: diskrw shmid [args path]
 *
 *  If [args path] is not supplied, diskrw functions as a clean up 
 *  agent of the shared memory segment. Otherwise, diskrw is 
 *  responsible for I/O to path.
 *
 *********************************************************************/
main(int argc, char* argv[])
{
    int	i;
    int	mode;
    svcport_t	*sp = 0;
    diskport_t	*dp = 0;
    bool	raw_disk = false;

    ppid = getppid();
    if (argc != 4 && argc != 2) {
	cerr << "usage: " << argv[0]
	     << " shmid [args path]"
	     << endl;
	fatal(__LINE__);
    }

#ifdef DEBUG_DISKRW
    cerr.form("diskrw: pid=%d:", getpid());
    for (i = 0; i < argc; i ++)
	cerr << ' ' << argv[i];
    cerr << endl;
#endif

    /*
     *  Attach to shared memory segment.
     */
    const char* arg1 = argv[1];
    istrstream(arg1) >> i;

    w_rc_t e = shmem_seg.attach(i);
    if (e)  {
	cerr << "diskrw:- problem with shared memory" << endl;
	cerr << e;
	fatal(__LINE__);
    }

    /*
     *  If this is a regular diskrw process, set up pointers to
     *  svcport and diskport.
     */
    if (argc == 4) {
	istrstream s(argv[2]);
	sp = (svcport_t*) (shmem_seg.base() + (s >> i, i));
	dp = (diskport_t*) (shmem_seg.base() + (s >> i, i));
	if( ! (sp->check_magic() && dp->check_magic()) ) {
	    cerr << 
	    "diskrw:- configurations of diskrw and server do not match" 
	    << endl;
	    fatal(__LINE__);
	}
	s >> fflags >> mode;
	if (! s)  {
	    cerr << "diskrw:- args argument is bad" << endl;
	    fatal(__LINE__);
	}
    }

    /*
     *  Set up signals/alarm
     */
    setup_signal();
    alarm(60);

    if (argc == 2)  {
	/*
	 *  This is the dummy diskrw-- the one that cleans up 
	 *  the shared memory. Loop forever.
	 */
	kill_siblings = 1;
	while (1)  pause();
	exit(0);
    }

#ifdef PIPE_NOTIFY
    /* close the pipe ends that we don't use */
     ::close(dp->rw_chan[PIPE_OUT]);
     ::close(dp->sv_chan[PIPE_IN]);
#endif

    /*
     * Close all file descriptors up to the limit. 
     */
    int fd;
    {
	int maxfds;

#if defined(_SC_OPEN_MAX)
	maxfds = (int) sysconf(_SC_OPEN_MAX);
#elif defined(RLIMIT_NOFILE)
	struct rlimit buf;
	if(getrlimit(RLIMIT_NOFILE, &buf)<0) {
	    perror("getrlimit(RLIMIT_NOFILE)");
	}
	maxfds = buf.rlim_cur; // SOFT limit
#else  
	maxfds = getdtablesize();
#endif
#ifdef notdef
	// could also try:
#    if defined(NOFILE)
	maxfds = NOFILE;
#    endif
#endif

	for (fd = 3; fd < maxfds; fd++)  {
#ifdef PIPE_NOTIFY
	    if (fd == dp->rw_chan[PIPE_IN] || fd == dp->sv_chan[PIPE_OUT])
		continue;
	    (void)::close(fd);
#else
	    (void)close(fd);
#endif
	}
    }
    
    /*
     *  Open the file.
     */
    fname = argv[3];
    fd = ::open(fname, fflags, mode);
    if (fd < 0) {
	cerr.form("diskrw: %s: open: %s\n", fname, strerror(errno));
	fatal(__LINE__);
    }

    /*
     *  If file is raw, then set up writebuf to cache writes
     *  for performance.
     */
#ifdef PREFETCH
#if defined(_SC_PAGESIZE)
    sys_pagesize = (int) sysconf(_SC_PAGESIZE);
#else
    sys_pagesize = getpagesize();
#endif
    if (sys_pagesize == -1) {
	    w_rc_t e = RC(stOS);
	    W_FORM(cerr)("diskrw: %s: pagesize unavailable, assuming 8 KB:\n",
			 fname);
	    cerr << e << endl;
	    sys_pagesize = 8192;
    }

    {
	const char *c = getenv("DISKREADTRACK");
	if (c) {
	    prefetch_on = true;
	    tracksize_in_pages = atoi(c);
	}
	else
	    tracksize_in_pages = 0;
    }
#endif /*PREFETCH*/
#ifdef PREFETCH_WARN
    if (getenv("DISKREADTRACK"))
	    W_FORM(cerr)("diskrw: %s: disk prefetch not supported\n");
#endif

    if (is_raw_device(fname)) {

	wbuf = new writebuf_t;
	if (! wbuf)  {
	    // we will make do without the buffer
	    cerr.form("diskrw: %s: Warning: no memory for write buffer: %s\n",
		      fname, strerror(errno));
	}

#ifdef PREFETCH
	if (prefetch_on) {
	    e = get_disk_info(fname, D);

	    if (e == RCOK) {
		    rbuf = new readbuf_t(fd);
		    if (! rbuf) {
			    cerr.form("diskrw: %s: Warning: no memory for read buffer: %s\n",
				      fname, strerror(errno));
			    cerr << "	... prefetch disabled" << endl;
			    prefetch_on = false;
		    }
	    }
	    else {
		    W_FORM(cerr)("diskrw: %s: can't get disk info for read buffer:\n", fname);
		    cerr << e << endl;
		    cerr << "	... prefetch disabled" << endl;
		    prefetch_on = false;
	    }
			    
		    
	}
#endif /*PREFETCH*/

	raw_disk = true;
    }
    /*
     *   If we are monitoring performance of diskrw processes,
     *   get the directory name in which to write the stats
     */

    {
	const char *c = getenv("DISKSTATSDIR");
	if(c) {
	    // const char *fname = "./volumes/dev1";
	    const char *z = strrchr(fname,'/');
	    
	    if(!z) {
	     	z = fname;
	    }
	    char *b = new char[strlen(c) + strlen(z)];
	    if(b) {
		strcpy(b,c);
		strcat(b,z);
		path = b;
	    } // else skip writing stats
	} // else skip writing stats

	// open & read it into s, u
	if(path) {
	    statfd = open(path, O_WRONLY | O_CREAT, smode);
	    if(statfd<0) {
		    perror("open");
		    cerr << "path=" << path <<endl;
		    exit(1);
	    }
	}
	u.start();
    }


    /*
     *  Loop forever and process disk I/O request.
     */
    while ( 1 )  {
	
	/*
	 *  Dequeue a message
	 */
	diskmsg_t m;
	dp->recv(m);
	diskv_t* diskv = m.diskv;

	/*
	 *  Convert shared memory offsets back to addresses
	 */
	struct	iovec iov[8];	// XXX magic number
	int	iovcnt;
	int	totalcc;
	int	cc;
	iovcnt = 0;
	totalcc = 0;
	switch (m.op) {
	case m.t_read:
	case m.t_write:
		iovcnt = m.dv_cnt;
		for (i = 0; i < m.dv_cnt; i++) {
			w_assert3((uint)diskv->bfoff < shmem_seg.size());
			iov[i].iov_base = shmem_seg.base() + diskv[i].bfoff;
			iov[i].iov_len = diskv[i].nbytes;
			totalcc += iov[i].iov_len;
		}
		break;
	default:
		break;
	}

	/*
	 *  Process message
	 */
	switch (m.op)  {
	case m.t_trunc:
	    /* the wbuf contents might overlap the truncated area */	
	    if (wbuf)
		flush_wbuf(fd);
	
	    s.ftruncs++;
	    if (ftruncate(fd, m.off) == -1) {
		cerr.form("diskrw: %s: ftruncate(%ld): %s\n",
			  fname, m.off, strerror(errno));
		fatal(__LINE__);
	    } 
	    break;

	case m.t_sync:
	    w_assert3(m.dv_cnt == 0);
	    if (wbuf) flush_wbuf(fd);

#if defined(AIX32) || defined(AIX41)
	    /* AIX doesn't allow fsync on devices.  To be precise,
	     it doesn't allow it on character devices.  I haven't
	     checked block devices --bolo */
	    if (raw_disk) {
		    s.fsyncs++;
		    break;
	    }
#endif	    
	    /* don't fsync() RO files */
	    if ((fflags & O_ACCMODE) == O_RDONLY) {
		    s.fsyncs++;
		    break;
	    }
		
	    if (fsync(fd) == -1) {
		cerr.form("diskrw: %s: fsync(%d): %s\n",
			  fname, fd, strerror(errno));
		fatal(__LINE__);
	    } 
	    s.fsyncs++;
	    break;

	case m.t_read:
	    w_assert3(m.dv_cnt > 0);
	    if (wbuf) flush_wbuf(fd);
	    {
		bool ok = false;

#ifdef PREFETCH
		if (rbuf && m.dv_cnt == 1) {
		    // for the time being, do this only if
		    // we have a one-part vector

		    wbufdatum_t d(m.off, iov[0].iov_base, iov[0].iov_len);
		    ok = rbuf->read_track(d);
		    // returns false if it failed
		} 
#endif /*PREFETCH*/

		if(!ok) {
		    if (lseek(fd, m.off, SEEK_SET) == -1)  {
			cerr.form("diskrw: %s: lseek(%ld): %s\n",
				  fname, m.off, strerror(errno));
			fatal(__LINE__);
		    }
#if 1
		    /* the real thing */
		    cc = readv(fd, iov, iovcnt);
		    if (cc != totalcc) {
			cerr.form("diskrw: %s: read(%d @ %ld)==%d: %s\n",
			      fname, totalcc, (long)m.off, cc,
			      errno ? strerror(errno) : "<no error>");
			fatal(__LINE__);
		    }
		    s.bread += totalcc;
#else
		    /* XXX left for debugging */
		    for (i = 0; i < iovcnt; i++) {
			cc = ::read(fd, iov[i].iov_base, iov[i].iov_len);
			if (cc != iov[i].iov_len) {
			    cerr.form("diskrw: %s: read(%d @ %ld)==%d: %s\n",
				  fname, iov[i].iov_len, (long)m.off, cc,
				  errno ? strerror(errno) : "<no error>");
			    fatal(__LINE__);
			}
			s.bread += iov[i].iov_len;
		    }
#endif
		    s.reads++;
		}
	    }
	    break;

	case m.t_write:
#ifdef PREFETCH
	    if (rbuf) {
		wbufdatum_t d(m.off, iov[i].iov_base, totalcc);
		rbuf->discard(d);
	    }
#endif /*PREFETCH*/

	    if (wbuf && iovcnt > 1)  {
		off_t off = m.off;
		for (i = 0; i < iovcnt; i++) {
		     wbufdatum_t d(off, (char *)iov[i].iov_base, iov[i].iov_len);
		
		     if (wbuf->deposit(d))  {
		        flush_wbuf(fd);
		        if (wbuf->deposit(d)) {
			    /* it's probably too big; just do it */
			    if (lseek(fd, d.offset, SEEK_SET) == -1)  {
				cerr.form("diskrw: %s: lseek(%ld): %s\n",
					  fname, d.offset, strerror(errno));
				fatal(__LINE__);
			    }
			    
			    cc = write(fd, d.ptr, d.len);
			    if (cc != d.len) {
				cerr.form("diskrw: %s: write(%d @ %ld)==%d: %s\n",
				      fname, d.len, (long)d.offset, cc,
				      errno ? strerror(errno) : "<no error>");
				fatal(__LINE__);
			    }
		        }
		    }
		    off += iov[i].iov_len;
		}
	    } else {

		if (wbuf) flush_wbuf(fd);
	    
		if (lseek(fd, m.off, SEEK_SET) == -1)  {
		    cerr.form("diskrw: %s: lseek(%ld): %s\n",
			      fname, m.off, strerror(errno));
		    fatal(__LINE__);
		}
#if 1
		/* do the real thing */
		cc = writev(fd, iov, iovcnt);
		if (cc != totalcc) {
			cerr.form("diskrw: %s: write(%d @ %ld)==%d: %s\n",
			      fname, totalcc, (long)m.off, cc,
			      errno ? strerror(errno) : "<no error>");
			fatal(__LINE__);
		}
		s.bwritten += totalcc;
#else
		for (i = 0; i < iovcnt; i++) {
		    cc = write(fd, iov[i].iov_base, iov[i].iov_len);
		    if (cc != iov[i].iov_len) {
			cerr.form("diskrw: %s: write(%d @ %ld)==%d: %s\n",
			      fname, iov[i].iov_len, (long)m.off, cc,
			      errno ? strerror(errno) : "<no error>");
			fatal(__LINE__);
		    }
		    s.bwritten += iov[i].iov_len;
	        }
#endif
		s.writes++;
	    }
	    break;
	default:
	    cerr.form("diskrw: %s: bad disk message op=%d\n", fname, m.op);
	    fatal(__LINE__);
	}

	/*
	 *  Begin atomic.
	 */
	dp->send(m);  // BOLO: switch these two statements!??
	sp->incr_incoming_counter(); 
	/*
	 *  End atomic.
	 */


	if (sp->sleep)  {
	    ShmcStats.kicks++;
	    /*
	     *  Kick server
	     */
#ifdef PIPE_NOTIFY
	    ChanToken token = 0;
	    if (::write(dp->sv_chan[PIPE_OUT], 
			(char *)&token, sizeof(token)) != sizeof(token)) {
		cerr.form("diskrw: %s: write token: %s\n",
			  fname, strerror(errno));
		fatal(__LINE__);
	    }
#else
	    if (kill(ppid, SIGUSR2) < 0) {
		cerr.form("diskrw: %s: kill(sm, SIGUSR2): %s\n",
			  fname, strerror(errno));
		fatal(__LINE__);
	    }
#endif
	}
    }
    fatal(__LINE__);
}



/*********************************************************************
 *
 *  clean_shutdown()
 *
 *********************************************************************/
void 
clean_shutdown()
{
    if (kill_siblings)  {
	// presumably siblings destroy their
	// shared memory segments
	diskport_t* dp = (diskport_t*) (shmem_seg.base() +
					sizeof(svcport_t));
	for (int i = 0; i < open_max; i++, dp++)  {
	    if (dp->pid && dp->pid != -1)  {
		kill(dp->pid, SIGTERM);
	    }
	}
    }

    w_rc_t e = shmem_seg.destroy();
    if (e)  {
	cerr << "diskrw (" << fname << "):- problem with shared memroy" << endl;
	cerr << e;
    }
    cerr << "diskrw (" << fname
        << "): server died" << endl;
    // cerr << ShmcStats << endl;

    // clean up all the server's left-over shm segments
    //
    FILE *res;
    bool failed=false;
    res = popen("ipcs -m -p | grep \"^m\"","r");
    if(res) {
	int n=7, id;
	pid_t owner;
	int last;
	char perm[20]; 
	char person[40];
	char group[40];
	char key[40];

	while(n==7) {
	    n = fscanf(res, "m %d %s %s %s %s %ld %d\n",
		&id, key, perm,  person, group, &owner, &last);

	    if(n == 7) {
		if(owner == ppid) {
		    cerr << "Removing segment " << id 
			<< " created by " << owner << endl;

		    if(shmctl(id,IPC_RMID,0) < 0) {
			perror("shmctl IPC_RMID");
			failed = true;
		    }
		}
#ifdef DEBUG_DISKRW
		else
		    cerr << "Ignoring segment " << id 
			<< " created by " << owner << endl;
#endif
	    } else if(n!=EOF) {
		failed = true;
	    }
	}
	pclose(res);
    } else {
	failed = true;
    }
    if(failed) {
	cerr << "diskrw (" << fname
            << "): Cannot clean up server's shared memory segments."
            <<endl
            << "diskrw (" << fname
            << "): You might have to do so by hand, with ipcs(1) and ipcrm(1)."
            << endl;
    }
#ifdef DEBUG_DISKRW
    cerr.form("diskrw: pid=%d: %s: normal exit.\n", getpid(), fname);
#endif
    exit(0);
}



/*********************************************************************
 *
 *  caught_signal(sig)
 *
 *********************************************************************/
void 
caught_signal(int sig)
{
    ShmcStats.lastsig = sig;
    switch (sig)  {
    case SIGALRM: 
	ShmcStats.alarm++; break;
    case SIGUSR1:
	ShmcStats.notify++; break;
    default:
	break;
    }
    switch (sig)  {
    case SIGALRM: 
    case SIGINT:
	if (getppid() != ppid || sig == SIGINT)
	    clean_shutdown();
	if(statfd > 0) writestats();
	alarm(60);
	break;
    case SIGTERM:
	    if (kill_siblings)
		    clean_shutdown();
	    /*FALLTHROUGH*/
    case SIGUSR2: {
	    // cerr << ShmcStats << endl;
	    w_rc_t e = shmem_seg.destroy();
	    if (e) {
		cerr << e << endl;
	    }
	    if (statfd > 0) writestats(true);
#if defined(DEBUG_DISKRW)
	    cerr.form("diskrw: pid=%d: %s: SIGTERM ination\n", getpid(), fname);
#endif
	    exit(-1);
	    break;
    }
    case SIGHUP:
	if(statfd > 0) writestats();

#ifdef DEBUG_DISKRW
	cerr.form("diskrw: pid=%d: %s: SIGHUP.\n", getpid(), fname);
#endif
	exit(0);
    case SIGUSR1:
	break;
    }
}



/*********************************************************************
 *
 *  setup_signal()
 *
 *********************************************************************/
void 
setup_signal()
{
    // allow all signals to be caught
    sigset_t all_mask;
    sigset_t proc_mask;

    sigemptyset(&all_mask);
    sigemptyset(&proc_mask);

    if (sigprocmask(SIG_SETMASK, &proc_mask, NULL) == -1)  {
	W_FATAL(stOS);
    }

#ifdef POSIX_SIGNALS
    // specify specific actions for some signals
    struct sigaction sact;
    sact.sa_handler = W_POSIX_HANDLER caught_signal;
    sact.sa_flags = 0;
#ifdef SA_RESTART
    sact.sa_flags |= SA_RESTART;
#endif
    if (sigaddset(&all_mask, SIGUSR1) == -1)  {
	W_FATAL(stOS);
    }
    if (sigaddset(&all_mask, SIGUSR2) == -1)  {
	W_FATAL(stOS);
    }

    if (sigaddset(&all_mask, SIGTERM) == -1) {
	W_FATAL(stOS);
    }

    if (sigaddset(&all_mask, SIGALRM) == -1)  {
	W_FATAL(stOS);
    }
#if 0
    if (sigaddset(&all_mask, SIGINT) == -1)  {
	W_FATAL(stOS);
    }
#endif

    if (sigaddset(&all_mask, SIGHUP) == -1)  {
	W_FATAL(stOS);
    }

    sact.sa_mask = all_mask;

    if (sigaction(SIGUSR1, &sact, 0) == -1) {
	W_FATAL(stOS);
    }
    if (sigaction(SIGUSR2, &sact, 0) == -1) {
	W_FATAL(stOS);
    }
    if (sigaction(SIGTERM, &sact, 0) == -1) {
	W_FATAL(stOS);
    }
    if (sigaction(SIGHUP, &sact, 0) == -1) {
	W_FATAL(stOS);
    }
#if 0
    if (sigaction(SIGINT, &sact, 0) == -1) { 
	// catch SIGINT from parent
	W_FATAL(stOS);
    }
#endif

    sact.sa_flags = 0;
#ifdef SA_INTERRUPT
    sact.sa_flags |= SA_INTERRUPT;
#endif
    if (sigaction(SIGALRM, &sact, 0) == -1) {
	W_FATAL(stOS);
    }

#if 1
    sact.sa_handler = W_POSIX_HANDLER SIG_IGN;
    sigemptyset(&sact.sa_mask);
    sact.sa_flags = 0;
    sigaction(SIGINT, &sact, 0);
#endif

/*
    sact.sa_flags = 0;
    sact.sa_handler = SIG_IGN;
    sigaction(SIGINT, &sact, 0);
*/
#else
    /* ANSI STANDARD C way -- allows int arg on handler */

#   define SETSIG(x)       {						\
    if (signal(x, W_ANSI_C_HANDLER caught_signal) == 			\
					W_ANSI_C_HANDLER SIG_ERR) {	\
	W_FATAL(stOS);							\
    }									\
}

    SETSIG(SIGUSR1);
    SETSIG(SIGTERM);
    SETSIG(SIGHUP);
    SETSIG(SIGINT);
    SETSIG(SIGALRM);
#endif /* !POSIX_SIGNALS*/

}

void
writestats(bool closeit)
{
    int cc;
    u.stop(1); // iters don't matter

    (void) lseek(statfd, 0, SEEK_SET);
    if((cc = write(statfd, &s, sizeof s)) != sizeof s ) {
	    perror("write s");
	    exit(1);
    }

    if((cc = write(statfd, &u, sizeof u)) != sizeof u ) {
	    perror("write u");
	    exit(1);
    }
    if(closeit) {
	(void) close(statfd);
    }
}
