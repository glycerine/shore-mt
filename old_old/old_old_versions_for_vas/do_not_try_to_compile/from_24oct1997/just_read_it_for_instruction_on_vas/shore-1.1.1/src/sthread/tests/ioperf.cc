/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: ioperf.cc,v 1.17 1997/09/26 18:57:23 solomon Exp $
 */
#include <iostream.h>
#include <strstream.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <memory.h>
#include <time.h>
#include <unix_error.h>
#ifdef SOLARIS2
#include <solaris_stats.h>
#else
#include <unix_stats.h>
#endif
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/uio.h>

#if !(defined(HPUX8) && defined(_INCLUDE_XOPEN_SOURCE)) && !defined(SOLARIS2) && !defined(AIX41)
extern "C" {
	int fsync(int);
	int ftruncate(int, off_t);
	int readv(int, struct iovec *, int);
}
#endif

#include <w.h>
#include <w_statistics.h>
#include <sthread.h>
#include <sthread_stats.h>

#define SECTOR_SIZE	512
#define BUFSIZE		1024

char* buf = 0;
int fastpath=0;

class io_thread_t : public sthread_t {
public:

    io_thread_t(
	char	rw_flag,
	const	char* file,
	size_t	block_size,
	int	block_cnt,
	char*	buf,
	bool    fastpath
	);

    ~io_thread_t();
    void 	rewind(int i);
    bool 	error() const { return _error;} 

protected:

    virtual void run();

private:

    char	_rw_flag;
    const char*	_fname;
    size_t	_block_size;
    int	 	_block_cnt;
    char*	_buf;
    bool 	_is_special;
    int 	_nblocks;
    bool 	_fastpath;
    int 	_fd;
    int 	_remote_fd;
    off_t	_offset;
    bool        _error;
    int         _total_bytes;
};

io_thread_t::io_thread_t(
    char rw_flag,
    const char* file,
    size_t block_size,
    int block_cnt,
    char* buf,
    bool    fastpath
    )
: _rw_flag(rw_flag),
  _fname(file),
  _block_size(block_size), _block_cnt(block_cnt),
  _buf(buf),
  _fastpath(fastpath),
  _remote_fd(0),
  _offset(0),
  _error(false),
  _total_bytes(0),
  sthread_t(t_regular, 0, 0, "io_thread")
{
}

io_thread_t::~io_thread_t()
{
}

void
io_thread_t::rewind( int )
{
    // for device files, skip over the first 2 sectors so we don't trash
    // the disk label
    w_rc_t rc;
    off_t off = 0;
    if(_is_special){
	off = 2 * SECTOR_SIZE;
    }
    if(_fastpath) {
	errno = 0;
	if( (off = ::lseek(_fd, off, SEEK_SET)) == -1) {
	    cerr << "lseek:" << endl << errno << endl;
	}
    } else {
	/* not a system call */
	rc = sthread_t::lseek(_fd, off, SEEK_SET, off);
	if(rc){
	    cerr << "lseek:" << endl << rc << endl;
	    W_COERCE(rc);
	}
    }
    // relative to end of sectors 1 & 2
    _offset = 0;

}

void
io_thread_t::run()
{
    stime_t timeBegin;
    stime_t timeEnd;
    struct stat stbuf;
    int oflag = O_RDONLY;
    const char* op_name = 0;
    w_rc_t rc;

    // see if it's a raw or character device
    if (stat(_fname, &stbuf))
	_is_special = false;
    else if ((stbuf.st_mode & S_IFMT) == S_IFCHR ||
	    (stbuf.st_mode & S_IFMT) == S_IFBLK)
	_is_special = true;
    else
	_is_special = false;

    if (_is_special) {
	// don't count the first two sectors.
	_nblocks = (stbuf.st_size - (2 * SECTOR_SIZE)) / _block_size;
    }
    else if (_rw_flag != 'r') 
	/* Don't adjust your file; we control the
	   horizontal AND the vertical. */
	_nblocks = _block_cnt;
    else {
	_nblocks = ((int)stbuf.st_size) / _block_size;
    }

    if (_nblocks < _block_cnt) {
	// Warn
	cerr << "Warning: file " << _fname << " has only " << _nblocks
		<< " blocks of size " << _block_size << "." <<endl;
	cerr << "Will have to re-read file to read " << _block_cnt
		<< " blocks." <<  endl;
    }

    // figure out what the open flags should be
    if(_rw_flag == 'r'){
	op_name = "read";
	oflag = O_RDONLY;
    }
    else if(_rw_flag == 'w') {
	op_name = "write";
	oflag = O_RDWR;
    }
    else if(_rw_flag == 'b') {
	op_name = "read/write";
	oflag = O_RDWR;
    }
    else
	cerr << "internal error at " << __LINE__ << endl;

    // some systems don't like O_CREAT with device files
    if(!_is_special) {
	oflag |= O_CREAT;
    }

    {
	rc = sthread_t::open(_fname, oflag, 0666, _fd);
	if(rc){
	    cerr << "open:" << endl << rc << endl;
	    W_COERCE(rc);
	}
    }
    // open the file locally, also
    if(_fastpath) {
	_remote_fd = _fd;
	errno = 0;
	_fd = ::open(_fname, oflag, 0666);
	if(_fd < 0) {
	    cerr << "open:" << endl << errno << endl;
	}
    } 

    // for device files, skip over the first 2 sectors so we don't trash
    // the disk label
    this->rewind(-1);

    cout << "starting: " << _block_cnt << " "
	 << op_name << " ops of " << _block_size
	 << " bytes each." << endl;

    iovec iov;

    timeBegin = stime_t::now(); /********************START ***************/

    for (int i = 0; i < _block_cnt; i++)  {
	if(i % _nblocks == (_nblocks-1) ) {
	    this->rewind(i);
	}

	if (_rw_flag == 'r' || _rw_flag == 'b') {
	    int cc;
	    if(_fastpath) {
		errno = 0;
		iov.iov_base = (caddr_t) _buf;
		iov.iov_len = _block_size;

		/*
		if ((cc = ::lseek(_fd, _offset, SEEK_SET)) != _offset)  {
		    cerr << "lseek (" << i << "/" << _offset << "):  returned " 
			<<  cc <<endl;
		    _error = true;
		    return;
		}
		*/

		if ((cc=::readv(_fd, &iov, 1)) != (int)_block_size)  {
		    cerr << "readv (" << i << "/" << _offset << "):  returned " 
			<< cc
			<< "; errno= " << errno << endl;
		    _error = true;
		    return;
		}
		/*
		if ((cc=::read(_fd, _buf, _block_size)) != _block_size)  {
		    cerr << "read (" << i << "/" << _offset << "):  returned " 
			<< cc
			<< "; errno= " << errno << endl;
		    _error = true;
		    return;
		}
		*/
	    } else {
		if (rc = sthread_t::read(_fd, _buf, _block_size))  {
		    cerr << "read:" << endl << rc << endl;
		    _error = true;
		    W_COERCE(rc);
		}
	    }
	}
	if (_rw_flag == 'w' || _rw_flag == 'b') {
	    if(_fastpath) {
		errno = 0;
		if (::write(_fd, _buf, _block_size) < 0)  {
		    cerr << "write (" << i << "/" << _offset << "): " 
		    << endl << errno << endl;
		    _error = true;
		    return;
		}
	    } else {
		if (rc = sthread_t::write(_fd, _buf, _block_size))  {
		    cerr << "write:" << endl << rc << endl;
		    _error = true;
		    W_COERCE(rc);
		}
	    }
	}
	_offset += _block_size;
	_total_bytes += _block_size;
    }
    timeEnd = stime_t::now(); /**************************** FINISH **********/

    if(_fastpath) {
	errno = 0;
	if(::fsync(_fd) < 0) {
	    cerr << "fsync:" << endl << errno << endl;
	}
    } else {
	W_COERCE( sthread_t::fsync(_fd) );
    }

    // timeEnd = stime_t::now(); 
    
    cout << "finished " 
	<< (char *)(_fastpath?"local":"remote") 
	<< " I/O :"
	<< _block_cnt << " "
	 << op_name << " ops of " << _block_size
	 << " bytes each; " 
	 << _total_bytes  << " bytes total. " 
	 << endl;

    if (_rw_flag == 'b') {
	_block_cnt <<= 1; // times 2
    }

    sinterval_t delta(timeEnd - timeBegin);

    cout << "Total time: " << delta << endl;

    float f =
	 (_block_size*_block_cnt)/(1000.0*1000.0) / (double)((stime_t)delta);
    cout << "MB/sec: "
	 << f
	 << endl;

    if(_fastpath) {
	errno = 0;
	if(::close(_fd) < 0) {
	    cerr << "close:" << endl << errno << endl;
	}
	_fd = _remote_fd;
    }

    W_COERCE( sthread_t::close(_fd) );
}

static unix_stats U1;
#ifndef SOLARIS2
static unix_stats U3(RUSAGE_CHILDREN);
#else
static unix_stats U3; // wasted
#endif


int
main(int argc, char** argv)
{

    U1.start();
    U3.start();
    if (argc != 5) {
	printf("usage: %s r(ead)/w(rite)/b(oth) filename block_size block_cnt\n", argv[0]);
	exit(1);
    }

    char rw_flag = argv[1][0];
    switch (rw_flag) {
    case 'R': case 'W': case 'B':
        fastpath++;
        rw_flag = (char)(((int) rw_flag) + 0x20);  // make lower-case
	cerr << "FAST PATH version of " << (char) rw_flag <<endl;
        break;
    case 'r': case 'w': case 'b':
	break;
    default:
	cerr << "first parameter must be read write flag [rRwWbB]"
	     << endl;
	return 1;
    }

    const char* fname = argv[2];
    size_t 	block_size = atoi(argv[3]);
    int  	block_cnt = atoi(argv[4]);

    char* 	buf = sthread_t::set_bufsize(block_size);


    io_thread_t thr(rw_flag, fname, block_size, block_cnt, buf, fastpath);
    W_COERCE( thr.fork() );
    W_COERCE( thr.wait() );

    if (!thr.error()) {
	cout << SthreadStats << endl;
    }

    U1.stop();
    U3.stop();
    if(!thr.error()) {
	cout << "Unix stats for parent:" << endl ;
	cout << U1 << endl <<endl;
#ifndef SOLARIS2
	cout << "Unix stats for children:" << endl;
	cout << U3 << endl <<endl;
#endif
    }
    return 0;
}
