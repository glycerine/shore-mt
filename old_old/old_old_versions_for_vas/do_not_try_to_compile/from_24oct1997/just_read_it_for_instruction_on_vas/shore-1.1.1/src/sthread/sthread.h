/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef STHREAD_H
#define STHREAD_H

#ifndef W_H
#include <w.h>
#endif

#include <w_timer.h>

class sthread_t;
class smthread_t;

#include <signal.h>

#include <setjmp.h>

/* The setjmp() / longjmp() used should NOT restore the signal mask.
   If sigsetjmp() / siglongjmp() is available, use them, as they
   guarantee control of the save/restore of the signal mask */

#if defined(SUNOS41) || defined(SOLARIS2) || defined(Ultrix42) || defined(HPUX8) || defined(AIX32) || defined(AIX41) || defined(Linux)
#define POSIX_SETJMP
#endif

#ifdef POSIX_SETJMP
#define	thread_setjmp(j)	sigsetjmp(j,0)
#define	thread_longjmp(j,i)	siglongjmp(j,i)
typedef	sigjmp_buf	thread_jmp_buf;
#else
#define	thread_setjmp(j)	_setjmp(j)
#define	thread_longjmp(j,i)	_longjmp(j,i)
typedef	jmp_buf		thread_jmp_buf;
#endif /* POSIX_SETJMP */

extern "C" {
#include "stcore.h"
#if defined(AIX32) || defined(AIX41)
#include <sys/select.h>
#endif
}

typedef w_list_t<sthread_t>		sthread_list_t;

#ifdef __GNUC__
#pragma interface
#endif

// in linux sys/wait.h defines WAIT_ANY
#if defined(Linux) && defined(WAIT_ANY)
#undef WAIT_ANY
#endif

#if 1
    /* XXX temporary hack to scope geometry stuff here */
#include "sdisk.hh"
#endif

class smutex_t;
class scond_t;
class sevsem_t;

/*
 * Normally these typedefs would be defined in the sthread_t class,
 * but due to bugs in gcc 2.5 we had to move them outside.
 * TODO: Fix for later g++ releases
 */
class ready_q_t;

class sthread_base_t : public w_base_t {
public:
    typedef uint4_t id_t;

    enum {
	max_thread 	= 32,
	WAIT_IMMEDIATE 	= 0, 
		// sthreads package recognizes 2 WAIT_* values:
		// == WAIT_IMMEDIATE
		// and != WAIT_IMMEDIATE.
		// If it's not WAIT_IMMEDIATE, it's assumed to be
		// a positive integer (milliseconds) used for the
		// select timeout.
		//
		// All other WAIT_* values other than WAIT_IMMEDIATE
		// are handled by sm layer.
		// 
		// The user of the thread (e.g., sm) had better
		// convert timeout that are negative values (WAIT_* below)
		// to something >= 0 before calling block().
		//
		//
	WAIT_FOREVER 	= -1,
	WAIT_ANY 	= -2,
	WAIT_ALL 	= -3,
	WAIT_SPECIFIED_BY_THREAD 	= -4, // used by lock manager
	WAIT_SPECIFIED_BY_XCT = -5, // used by lock manager
    };

    static const w_error_t::info_t 	error_info[];
	static void  init_errorcodes();

#include "st_error.h"

    enum {
	stOS = fcOS,
	stINTERNAL = fcINTERNAL,
	stNOTIMPLEMENTED = fcNOTIMPLEMENTED,
    };
    /* XXX sdisk stuff hung into sthread for now */
    enum {
	    OPEN_RDWR = O_RDWR,
	    OPEN_RDONLY = O_RDONLY,
	    OPEN_WRONLY = O_WRONLY,
	    OPEN_TRUNC = O_TRUNC,
	    OPEN_CREATE = O_CREAT,
	    OPEN_SYNC = O_SYNC,
	    OPEN_EXCL = O_EXCL
    };

    /* additional open flags which shouldn't collide with unix flags */
    enum {
	OPEN_LOCAL = 0x10000000,	// do I/O locally
	OPEN_KEEP  = 0x20000000,	// keep fd open
	OPEN_FAST  = 0x40000000 	// open for fastpath I/O
    };
    enum {
	IOF_LOCAL = 0x1,		// do I/O locally
	IOF_KEEP = 0x2,			// FD open locally
	IOF_FASTPATH= 0x4		// fd open for fastpath I/O	
    };
    /* XXX if this doesn't match the native iovec, the I/O code
       will need to convert between shore and kernel formats. */
    typedef struct iovec iovec;
    enum { iovec_max = 8 };	/* XXX magic number from diskrw */
};

#define NAME_ARRAY 64
class sthread_name_t {
public:
    void rename(const char *n1, const char *n2=0, const char *n3=0);
    NORET			sthread_name_t();
    NORET			~sthread_name_t();
    char 			_name[NAME_ARRAY];
    NORET           W_FASTNEW_CLASS_PTR_DECL;
};

class sthread_named_base_t: public sthread_base_t  {
public:
    NORET			sthread_named_base_t(
	const char*		    n1 = 0,
	const char*		    n2 = 0,
	const char*		    n3 = 0);
    NORET			~sthread_named_base_t();
    
    void			rename(
	const char*		    n1,
	const char*		    n2 = 0,
	const char*		    n3 = 0);

    const char*			name() const;
    void			unname();

private:
    sthread_name_t*		_name;
};

inline NORET
sthread_named_base_t::sthread_named_base_t(
    const char*		n1,
    const char*		n2,
    const char*		n3)
	: _name(0)
{
    if (n1) rename(n1, n2, n3);

}

inline const char*
sthread_named_base_t::name() const
{
    return _name ? _name->_name : ""; 
}

class sthread_main_t;
class sthread_idle_t;
class sfile_handler_t;
class sfile_hdl_base_t;

class ThreadFunc
{
    public:
	virtual void operator()(const sthread_t& thread) = 0;
};


#include "strace.h"
class _strace_t;

/*
 *  Thread Structure
 */
class sthread_t : public sthread_named_base_t  {
    friend class sthread_init_t;
    friend class sthread_priority_list_t;
    friend class sthread_idle_t;
    friend class sthread_main_t;
    friend class sthread_timer_t;
#ifndef OLD_SM_BLOCK
    /* For access to block() and unblock() */
    friend class smutex_t;
    friend class scond_t;
    friend class diskport_t;
#endif
    
public:

    enum status_t {
	t_defunct,	// thread has terminated
	t_virgin,	// thread hasn't started yet	
	t_ready,	// thread is ready to run
	t_running,	// when me() is this thread 
	t_blocked,      // thread is blocked on something
	t_boot		// system boot
    };
    static char *status_strings[];

    enum priority_t {
	t_time_critical = 3,
	t_regular	= 2,
	t_fixed_low	= 1,
	t_idle_time 	= 0,
	max_priority    = t_time_critical,
	min_priority    = t_idle_time,
    };
    static char *priority_strings[];

    /* Default stack size for a thread */
    enum { default_stack = 64*1024 };

    /*
     *  Class member variables
     */
    void* 			user;	// user can use this 

    const id_t 			id;

    int				trace_level;	// for debugging

#ifndef OLD_SM_BLOCK
private:
#endif
    /*
       Block() and unblock() do not belong in the user-visible interface.
       They are scheduled for deletion in the near future.
     */
    static w_rc_t		block(
	int4_t 			    timeout = WAIT_FOREVER,
	sthread_list_t*		    list = 0,
	const char* const	    caller = 0,
	const void *		    id = 0);
    w_rc_t			unblock(const w_rc_t& rc = *(w_rc_t*)0);
#ifndef OLD_SM_BLOCK
public:
#endif

    virtual void		_dump(ostream &); // to be over-ridden
    static void			dump(const char *, ostream &); // traverses whole list
    static void                 dump_stats(ostream & o);
    static void                 reset_stats();
    static void			find_stack(void *address);
    static void			for_each_thread(ThreadFunc& f);

    w_rc_t			set_priority(priority_t priority);
    priority_t			priority() const;
    status_t			status() const;
    w_rc_t			set_use_float(int);
    void			push_resource_alloc(const char* n,
						    const void *id = 0,
						    bool is_latch = false);
    void			pop_resource_alloc(const void *id = 0);

    /*
     *  Concurrent I/O ops
     */
    static char*		set_bufsize(int size);
    static w_rc_t		open(
	const char*		    path,
	int			    flags,
	int			    mode,
	int&			    fd);
    static w_rc_t		close(int fd);
    static w_rc_t		writev(
	int 			    fd,
	const iovec*	    	    iov,
	size_t 			    iovcnt);
    static w_rc_t		write(
	int 			    fd, 
	const void* 		    buf, 
	int 			    n);
    static w_rc_t		read(
	int 			    fd,
	const void* 		    buf,
	int 			    n);
    static w_rc_t		readv(
	int 			    fd, 
	const iovec* 		    iov,
	size_t			    iovcnt);
    static w_rc_t		lseek(
	int			    fd,
	off_t			    offset,
	int			    whence,
	off_t&			    ret);
    /* returns an error if the seek doesn't match its destination */
    static w_rc_t		lseek(
	int			    fd,
	off_t			    offset,
	int			    whence);
    static w_rc_t		fsync(int fd);
    static w_rc_t		ftruncate(int fd, off_t sz);
    static w_rc_t		fstat(int fd, struct stat &sb);
    static w_rc_t		fgetfile(int fd, void *);
    static w_rc_t		fgetgeometry(int fd, struct disk_geometry &dg);
    static w_rc_t		fisraw(int fd, bool &raw);
    static	int		_io_in_progress;


    /*
     *  Misc
     */
    static sthread_t*		me()  { return _me; }

    /* XXX  sleep, fork, and wait exit overlap the unix version. */

    // sleep for timeout milliseconds
    void			sleep(long timeout = WAIT_IMMEDIATE,
				      char *reason = 0);
    void			wakeup();

    // wait for a thread to finish running
    w_rc_t			wait(long timeout = WAIT_FOREVER);

    // start a thread
    w_rc_t			fork();

    // exit the current thread
    static void 		end();
    static void			exit() { end(); }

    // give up the processor
    static void			yield(bool doselect=false);

    static void			set_diskrw_name(const char* diskrw)
					{_diskrw = diskrw;}

    ostream			&print(ostream &) const;

    unsigned			stack_size() const;

    // anyone can wait and delete a thread
    virtual			~sthread_t();

    static w_rc_t		io_start(sfile_hdl_base_t &);
    static w_rc_t		io_finish(sfile_hdl_base_t &);
    static bool			io_active(int fd);

    // function to do runtime up-cast to smthread_t
    // return 0 if the sthread is not derrived from sm_thread_t.
    // should be removed when RTTI is supported
    virtual smthread_t*		dynamic_cast_to_smthread();
    virtual const smthread_t*	dynamic_cast_to_const_smthread() const;

protected:
    NORET			sthread_t(
	priority_t		    priority = t_regular,
	bool			    block_immediate = false,
	bool			    auto_delete = false,
	const char*		    name = 0,
	unsigned		    stack_size = default_stack);

#ifdef notyet
    sthread_t(priority_t	priority = t_regular,
	      const char	*name = 0,
	      unsigned		stack_size = default_stack);
#endif

    virtual void		run() = 0;

private:

    friend class pipe_handler;
    friend class ready_q_t;

    static ready_q_t*		_ready_q;	// ready queue

    enum {
	fd_base = 4000,
	/* XXX temporary kludge; must match open_max in diskrw.h */
	sthread_open_max = 32
    };

    // thread resource tracing
    strace_t	_trace;
    _strace_t	*_blocked;	// resource thread is blocked for

    sevsem_t*			_terminate;	// posted when thread ends

    sthread_core_t		_core;		// registers, stack, etc
    status_t			_status;	// thread status
    priority_t			_priority; 	// thread priority
    thread_jmp_buf		_exit_addr;	// longjmp to end thread
    w_rc_t			_rc;		// used in block/unblock

    w_link_t			_link;		// used by smutex etc

    w_link_t			_class_link;	// used in _class_list
    static sthread_list_t*	_class_list;

    sthread_t*			_ready_next;	// used in ready_q_t
    sthread_timer_t*		_tevent;	// used in sleep/wakeup

    static	unsigned char	_io_flags[sthread_open_max];	    
    static w_rc_t		_io(
	int 			    fd, 
	int 			    op,
	const 			    iovec*, 
	int			    cnt,
	off_t                       other=0);

    /* in-thread startup and shutdown */ 
    static void			__start(void *arg_thread);
    void			_start();

    /* system initialization and shutdown */
    static w_rc_t		startup();
    static w_rc_t		shutdown();
    static w_rc_t		setup_signals(sigset_t &lo_spl,
					      sigset_t &hi_spl);

    static void 		_caught_signal(int);
    static void 		_ctxswitch(status_t s); 
    static void			_polldisk();

    static const char*		_diskrw;	// name of diskrw executable
    static pid_t		_dummy_disk;
    static sthread_t*		_me;		// the running thread
    static sthread_t*		_idle_thread;
    static sthread_t*		_main_thread; 
    static uint4_t		_next_id;	// unique id generator
    static w_timerqueue_t	_event_q;
    static sfile_handler_t	*_io_events;
    

    /* Control for idle thread dynamically changing priority */
    static int			_idle_priority_phase;
    static int			_idle_priority_push;
    static int			_idle_priority_max;
};


extern ostream &operator<<(ostream &o, const sthread_t &t);
extern ostream &operator<<(ostream &o, const sthread_core_t &c);
void print_timeout(ostream& o, const long timeout);


class sthread_main_t : public sthread_t  {
    friend class sthread_t;
	
protected:
    NORET			sthread_main_t();
    virtual void		run();
};

class sthread_idle_t : public sthread_t {
private:
    friend class sthread_t;

protected:
    NORET			sthread_idle_t();
    virtual void		run();
};

class sthread_priority_list_t 
    : public w_descend_list_t<sthread_t, sthread_t::priority_t>  {
public:
    NORET			sthread_priority_list_t(); 
	
    NORET			~sthread_priority_list_t()   {};
private:
    // disabled
    NORET			sthread_priority_list_t(
	const sthread_priority_list_t&);
    sthread_priority_list_t&	operator=(const sthread_priority_list_t&);
};

/*
 *  Mutual Exclusion
 */
class smutex_t : public sthread_named_base_t {
public:
    NORET			smutex_t(const char* name = 0);
    NORET			~smutex_t();
#ifdef DEBUG
    w_rc_t			acquire(int4_t timeout = WAIT_FOREVER);
#else
    w_rc_t			acquire(int4_t timeout);
    w_rc_t			acquire();
#endif
    void			release();
    bool			is_mine() const;
    bool			is_locked() const;

private:
    sthread_t*			holder;	// owner of the mutex
    sthread_priority_list_t	waiters;

    // disabled
    NORET			smutex_t(const smutex_t&);
    smutex_t&			operator=(const smutex_t&);
};

#ifndef NOT_PREEMPTIVE
#define MUTEX_ACQUIRE(mutex)    W_COERCE((mutex).acquire());
#define MUTEX_RELEASE(mutex)    (mutex).release();
#define MUTEX_IS_MINE(mutex)    (mutex).is_mine()
#else
#define MUTEX_ACQUIRE(mutex)
#define MUTEX_RELEASE(mutex)
#define MUTEX_IS_MINE(mutex)    1
#endif

/*
 *  Condition Variable
 */
class scond_t : public sthread_named_base_t  {
public:
    NORET			scond_t(const char* name = 0);
    NORET			~scond_t();

    w_rc_t			wait(
	smutex_t& 		    m, 
	int4_t			    timeout = WAIT_FOREVER);

    void 			signal();
    void 			broadcast();
    bool			is_hot() const;

private:
    sthread_priority_list_t	_waiters;

    // disabled
    NORET			scond_t(const scond_t&);
    scond_t&			operator=(const scond_t&);
};

/*
 *  event semaphore
 */
class sevsem_t : public sthread_base_t {
public:
    NORET			sevsem_t(
	int 			    is_post = 0,
	const char* 		    name = 0);
    NORET			~sevsem_t();

    w_rc_t			post();
    w_rc_t			reset(int* pcnt = 0);
    w_rc_t			wait(int4_t timeout = WAIT_FOREVER);
    void 			query(int& pcnt);
    
    void	 		setname(
	const char* 		    n1, 
	const char* 		    n2 = 0);
private:
    smutex_t 			_mutex;
    scond_t 			_cond;
    uint4_t			_post_cnt;

    // disabled
    NORET			sevsem_t(const sevsem_t&);
    sevsem_t&			operator=(const sevsem_t&);
};

inline bool
smutex_t::is_mine() const
{ 
    return holder == sthread_t::me(); 
}

inline bool
smutex_t::is_locked() const
{
    return holder != 0;
}

/*
 *  Internal --- responsible for initialization
 */
class sthread_init_t : public sthread_base_t {
public:
    NORET			sthread_init_t();
    NORET			~sthread_init_t();
private:
    static uint4_t 		count;
};

//////////////////////////////////////////////////////
// In sthread_core.c sthread_init_t must be the 2nd
// thing initialized; in every other file it
// must be the first.
//////////////////////////////////////////////////////
#ifndef STHREAD_CORE_C
static sthread_init_t sthread_init;
#endif /*STHREAD_CORE_C*/


inline sthread_t::priority_t
sthread_t::priority() const
{
    return _priority;
}

inline sthread_t::status_t
sthread_t::status() const
{
    return _status;
}

inline bool
scond_t::is_hot() const 
{
    return ! _waiters.is_empty();
}

inline void
sevsem_t::setname(const char* n1, const char* n2)
{
    _mutex.rename("s:m:", n1, n2);
    _cond.rename("s:c:", n1, n2);
}


/* include sfile stuff for backwards-compatability */
#include "sfile_handler.h"


inline    void	sthread_t::push_resource_alloc(const char* n,
					       const void *id,
					       bool is_latch)
{
	_trace.push(n, id, is_latch);
}
				
inline	void	sthread_t::pop_resource_alloc(const void *id)
{
	_trace.pop(id);
}
#endif /* STHREAD_H */

