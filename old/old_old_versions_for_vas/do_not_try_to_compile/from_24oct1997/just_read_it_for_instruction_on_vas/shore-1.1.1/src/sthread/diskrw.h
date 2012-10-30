/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 * $Id: diskrw.h,v 1.39 1997/09/06 22:39:18 solomon Exp $
 */
#ifndef DISKRW_H

#define DISKRW_H

// define this to volatile if the compiler supports it
#define VOLATILE 

#if defined(I860)
#define	PIPE_NOTIFY
#endif

#ifdef	PIPE_NOTIFY
#include <errno.h>
extern "C" void perror(const char *);
#endif /* PIPE_NOTIFY */


/* 
 * Magic numbers used to verify that the server and diskrw agree
 * on the communication protocol and layout of shared memory.
 * Increment the last part of the magic number if you make
 * any change to the size or shape of any of the data structures:
 */
#ifdef PIPE_NOTIFY
    enum { diskrw_magic = 0x7adbda03 };
#else
    enum { diskrw_magic = 0x7edbde03 };
#endif /* PIPE_NOTIFY */

class sthread_t;

#include "shmc_stats.h"

const open_max = 32;
const max_diskv = 8;

struct diskv_t {
    long	bfoff;
    int		nbytes;
};


/*
   A message between the thread package and I/O processes which
   communicate I/O requests and completions.
 */

struct diskmsg_t {
    enum op_t { t_read, t_write, t_sync, t_trunc };
    op_t	op;
    sthread_t*	thread;
    off_t	off;
    int		dv_cnt;
    diskv_t	diskv[max_diskv];

    diskmsg_t()	{};
    diskmsg_t& operator=(const diskmsg_t& m)  {
	op = m.op;
	thread = m.thread;
	off = m.off;
	dv_cnt = m.dv_cnt;
	if (dv_cnt == 1) 
	    diskv[0] = m.diskv[0];
	else 
	    memcpy(diskv, m.diskv, sizeof(diskv_t) * dv_cnt);
	return *this;
    }

#ifdef STHREAD_C
    diskmsg_t(op_t o, off_t offset, const diskv_t* v, int cnt) 
	: op(o), thread(sthread_t::me()), off(offset), dv_cnt(cnt)
    {
	w_assert3(cnt>=0 && cnt <= max_diskv);
	memcpy(diskv, v, sizeof(diskv_t) * cnt);
    }
	
    diskmsg_t(op_t o, off_t offset, long bfoffset, int n)
    : op(o), thread(sthread_t::me()), off(offset), dv_cnt(1)
    {
	diskv[0].bfoff = bfoffset;
	diskv[0].nbytes = n;
    }
#endif
};


/*
   A circular message queue is used to pass I/O requests
   between a I/O process and the thread package.

   The message queue synchronizes access with other processes
   (and threads) via a spinlock.

   XXX The '_outstanding' variable is redundant with regards to the
   'cnt' of items in the message queue (should be fixed).

   XXX Placing and removing messages from the queue is disparate
   from verifying queue consistency.  It requires that the user of
   the message queue lock the queue's lock.  This is a potential hazard
   which could be fixed.  
 */  

class msgq_t {
public:
    VOLATILE int		_outstanding;
    VOLATILE spinlock_t	lock;
private:
    enum { qsize = 12 };
    diskmsg_t	msg[qsize];
    int 	head, tail, cnt;
public:
    msgq_t() : _outstanding(0), head(0), tail(0), cnt(0) {};
    ~msgq_t()	{};

    outstanding() const  { return _outstanding; }

    void put(const diskmsg_t& m, bool getlock=true) {
	w_assert1(cnt < qsize);
	if(getlock) {
	    ACQUIRE(lock);	
	}
	msg[tail]= m, ++cnt;
	if (++tail >= qsize) tail = 0;
	++_outstanding; // not used
	if(getlock) {
	    lock.release();
	}
    }
    void get(diskmsg_t& m,bool getlock=true) {
	if(getlock) {
	    ACQUIRE(lock); // SPUN in server recving reply
	}
	w_assert3(cnt > 0);
	m = msg[head], --cnt;
	if (++head >= qsize) head = 0;
	--_outstanding;
	if(getlock) {
	    lock.release();
	}
    }
    is_empty()	{ return cnt == 0; }
    is_full()	{ return cnt == qsize; }
};


/*
   A service port is another shared memory data structure. 
   It is multiplexed between all i/o processes, and tells
   the thread package if any I/O compoletions are outstanding.

   In addition, it attempts to retain sanity by verifying that
   the I/O process and the thread package both have the same
   version of the shared-memory data structures compiled in.
 */

class svcport_t {

private:
    VOLATILE int		incoming;	// disk op reply pending
    VOLATILE spinlock_t	lock;

public:
    VOLATILE int		sleep;		// server sleep

private:
    // NOTE: be sure to increment diskrw_magic if you change this
    //       data structure.
    enum	{ magic = diskrw_magic };
    const int	_magic;

public:
    check_magic()	{ return magic == _magic; }
    svcport_t() : incoming(0), sleep(0), _magic(magic) {
	    if(!check_magic()) {
		cerr << 
		"Configurations of diskrw and server do not match." 
		<< endl;
	    }
	};

#ifdef DISKRW_C
    void 	incr_incoming_counter()  {
	ACQUIRE(lock);
	++incoming;
	lock.release();
    }
#endif
#ifdef STHREAD_C
    int		peek_incoming_counter() {
	return incoming;
    }
    void	decr_incoming_counter(int /*count*/) {
	ACQUIRE(lock);
#if 1  /*** uncomment 'count' above if this is set to false (0) ***/
	    // avoid choking on unavoidable race condition 
        incoming = 0;
#else
        incoming -= count;
        w_assert3(incoming >= 0);
#endif
	lock.release();
    }
#endif

};

#ifdef PIPE_NOTIFY
typedef	int	ChanToken;
enum { PIPE_IN = 0, PIPE_OUT = 1 };
#endif /* PIPE_NOTIFY */


/*
   The disk port is used by the thread package and the I/O process
   to communicate their I/O requests and completions.

   It uses two message queues, one for thread to I/O requests,
   and a second for I/O to thread completions.
 */

class diskport_t {
private:
    // NOTE: be sure to increment diskrw_magic if you change this
    //       data structure.
    enum { magic = diskrw_magic };
#ifdef DISKRW_C
    enum { IN = 0, OUT = 1 };
#else
    enum { IN = 1, OUT = 0 };
#endif
    VOLATILE msgq_t	queue[2];
    VOLATILE int		sleep;
public:
    off_t	pos;
    pid_t	pid;
#ifdef PIPE_NOTIFY
    int		rw_chan[2];	/* channel to notify diskrw on 		*/
    int		sv_chan[2];	/* channel to notify server on 		*/
#endif /* PIPE_NOTIFY */
    const int	_magic; /* magic should be last so that data structure
				   size differences are detected 	*/
    int	        _fd;    /* for server's fastpath                        */

    diskport_t() : sleep(0), pos(0), pid(0), _magic(magic) {};
    ~diskport_t()	{
	pos = 0, pid = 0, sleep = 0;
#ifdef PIPE_NOTIFY
	rw_chan[0] = rw_chan[1] = -1;
	sv_chan[0] = sv_chan[1] = -1;
#endif /* PIPE_NOTIFY */
    }

    outstanding() const  { 
		return queue[0].outstanding()+queue[1].outstanding();
	}

    void send(const diskmsg_t& m);
    void recv(diskmsg_t& m);
    int send_ready()	{ return !queue[OUT].is_full(); }
    int recv_ready()	{ return !queue[IN].is_empty(); }

    check_magic()	{ return magic == _magic; }
};

#ifdef DISKRW_C

void diskport_t::send(const diskmsg_t& m)
{
    queue[OUT].put(m);
}

void diskport_t::recv(diskmsg_t& m)
{
    ACQUIRE(queue[IN].lock);		// SPUN in diskrw recving request

    sleep = 1;  // may sleep, set set this now to avoid race condition
    if (queue[IN].is_empty())  {
#ifndef PIPE_NOTIFY
	sigset_t	mask, oldmask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGUSR1);
	int kr = sigprocmask(SIG_BLOCK, &mask, &oldmask);
	if (kr == -1) {
		cerr << "sigprocmask(BLOCK SIGUSR1)"
			<< ::strerror(::errno) << endl;
		::_exit(1);
	}
#endif
	queue[IN].lock.release();
	ShmcStats.lastsig = 0;
#ifdef PIPE_NOTIFY
	ChanToken token = 0;
	int	n;
	void clean_shutdown();
	while (queue[IN].is_empty()) {
		errno = 0;
		n = read(rw_chan[PIPE_IN], (char *)&token, sizeof(token));
		if (n == -1 && errno == EINTR)	/* intr syscall */
			continue;
		else if (n == 0) 	/* no more writers */
			clean_shutdown();
		else if (n != sizeof(token)) {
			::perror("diskrw:- read channel token");
			//cerr << "    additional info: n = " << n << endl;
			exit(1);
		}
		ShmcStats.notify++;
		ShmcStats.lastsig = SIGUSR1; // pretend
	}
#else /* PIPE_NOTIFY */
	while (queue[IN].is_empty()) {
		int kr = sigsuspend(&oldmask);
		if (kr == -1  &&  errno != EINTR) {
			cerr << "sigsuspend(): " <<
				::strerror(::errno) << endl;
			::_exit(1);
		}
	}
#endif /* PIPE_NOTIFY */
	switch(ShmcStats.lastsig){
	case SIGALRM: ShmcStats.falarm++; break;
	case SIGUSR1: ShmcStats.fnotify++; break;
	case 0: ShmcStats.found++; break;
	default:
		w_assert3(0);
	}

#ifndef PIPE_NOTIFY
	kr = sigprocmask(SIG_SETMASK, &oldmask, 0);
	if (kr == -1) {
		cerr << "sigprocmas(SIG_SETMASK): "
			<< ::strerror(::errno) << endl;
		::_exit(1);
	}
#endif

	ACQUIRE(queue[IN].lock);
    }
    sleep = 0;
    queue[IN].get(m,false);
    queue[IN].lock.release();
}

#endif /*DISKRW_C*/

#ifdef STHREAD_C

void diskport_t::send(const diskmsg_t& m)
{
    ACQUIRE(queue[OUT].lock);
    while (queue[OUT].is_full())  {
	queue[OUT].lock.release();
	sthread_t::yield();
	ACQUIRE(queue[OUT].lock);
    }
    queue[OUT].put(m,false);
    int kick = sleep;
    queue[OUT].lock.release();
    if (kick)  {
	ShmcStats.kicks++;
#ifdef PIPE_NOTIFY
	ChanToken token = 0;
	if (::write(rw_chan[PIPE_OUT], (char *)&token, sizeof(token)) 
							!= sizeof(token)) {
		::perror("write channel token");
		::exit(-1);
	}
#else
	if (kill(pid, SIGUSR1) == -1) {
	    W_FATAL(fcOS);
	}
#endif
    }
    
    sthread_t::priority_t oldp = sthread_t::me()->priority();
    W_COERCE( 
	sthread_t::me()->set_priority(sthread_t::t_time_critical) );
    
    int befores = SthreadStats.selects;

    SthreadStats.iowaits++;

    W_COERCE( sthread_t::block(sthread_t::WAIT_FOREVER, 0,
			       "sthread_t::_io") );
    int	afters = SthreadStats.selects;

    switch(afters-befores) {
	case 0: SthreadStats.zero++; break;
	case 1: SthreadStats.one++; break;
	case 2: SthreadStats.two++; break;
	case 3: SthreadStats.three++; break;
	default:
		if(afters>befores) {
		    SthreadStats.more++;
		} else {
		    SthreadStats.wrapped++;
		}
		break;
    }
    
    W_COERCE( sthread_t::me()->set_priority(oldp) );
}

void diskport_t::recv(diskmsg_t& m)
{
    queue[IN].get(m);
}
#endif /*STHREAD_C*/

#ifdef DISKRW_C
const char *shmc_stats::stat_names[1]; // not used by diskrw
#endif

#if !(defined(HPUX8) && defined(_INCLUDE_XOPEN_SOURCE)) && !defined(Linux)
extern "C" {
	int fsync(int);
	int ftruncate(int, off_t);
}
#endif
#endif /*DISKRW_H*/
