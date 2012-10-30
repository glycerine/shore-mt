/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#include <strstream.h>
#include <w_workaround.h>
#include <w_signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/time.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <new.h>
#include <sys/uio.h>

#ifdef __GNUC__
#pragma implementation "sfile_handler.h"
#endif

#define DBGTHRD(arg)
#define DBG(arg)
#define FUNC(arg)

#define W_INCL_LIST
#include <w.h>
#include <sthread.h>
#include <sfile_handler.h>

#include <w_statistics.h>
#ifdef SOLARIS2
#include <solaris_stats.h>
#else
#include <unix_stats.h>
#endif
#include "sthread_stats.h"
#include <unix_error.h>
extern class sthread_stats SthreadStats;

#ifdef __GNUC__
template class w_list_t<sfile_hdl_base_t>;
template class w_list_i<sfile_hdl_base_t>;
#endif


#define FD_NONE	-1	/* invalid unix file descriptor */

#ifdef HPUX8
inline int select(int nfds, fd_set* rfds, fd_set* wfds, fd_set* efds,
	      struct timeval* t)
{
    return select(nfds, (int*) rfds, (int*) wfds, (int*) efds, t);
}
#else

#if !defined(AIX32) && !defined(AIX41)
extern "C" int select(int, fd_set *, fd_set *, fd_set *, struct timeval *);
#endif

#endif /*HPUX8*/

/*
 *  operator<<(o, fd_set)
 *
 *  Print an fd_set.
 */

ostream& operator<<(ostream& o, const fd_set &s) 
{
#ifdef FD_SETSIZE
	int	maxfds = FD_SETSIZE;
#else
	static int maxfds = 0;
	if (maxfds == 0) {
		maxfds = sysconf(_SC_OPEN_MAX);
	}
#endif
	for (int n = 0; n < maxfds; n++)  {
		if (FD_ISSET(n, &s))  {
			o << ' ' << n;
		}
	}
	return o;
}

#ifdef DEBUG
/*********************************************************************
 *
 *  check for all-zero mask
 *
 *  For debugging.
 *
 *********************************************************************/

bool
fd_allzero(const fd_set *s)
{
#ifdef FD_SETSIZE
	int	maxfds = FD_SETSIZE;
#else
	static long maxfds = 0;
	if (maxfds == 0) {
		maxfds = sysconf(_SC_OPEN_MAX);
	}
#endif

	// return true if s is null ptr or it
	// points to an empty mask
	if (s==0)
		return true;

	for (int n = 0; n < maxfds; n++)  {
		if (FD_ISSET(n, s))
			return false;
	}
	return true;
}

#endif /*DEBUG*/

#ifdef DEBUG
static int gotenv;	// hold over from sthreads integration
#endif


/*
 * File "Handlers" and "Handles".
 *
 * A means of generating thread wakeup events for thread's
 * awaiting I/O on non-blocking devices.
 */


sfile_handler_t::sfile_handler_t()
: _list(offsetof(sfile_hdl_base_t, _link))
{
	memset(masks, '\0', sizeof(masks));
	memset(ready, '\0', sizeof(ready));

	any[0] = any[1] = any[2] = 0;

	direction = 0;
	rc = wc = ec = 0;

	dtbsz = 0;

	tvp = 0;
	tval.tv_sec = tval.tv_usec = 0;
}


/*
 *  sfile_handler_t::enable()
 *
 * Allow this sfile to receive events.
 */

w_rc_t sfile_handler_t::start(sfile_hdl_base_t &hdl)
{
	DBG(<<"sfile_hdl_t::enable(" << hdl << ")");

	if (hdl.fd == FD_NONE)
		return RC(sthread_t::stBADFD);

	// already enabled?
	void *owner = hdl._link.member_of();
	if (owner) {
		/* enabled by a different handler?! */
		if (owner != &_list)
			return RC(sthread_t::stINUSE);
		return RCOK;
	}

	_list.append(&hdl);
	hdl._owner = this;
	
	if (hdl.fd >= dtbsz)
		dtbsz = hdl.fd + 1;

	return RCOK;
}


/*
 *  sfile_handler_t::disable()
 *
 *  Stop dispatching events for a sfile.
 */

void sfile_handler_t::stop(sfile_hdl_base_t &hdl)
{
	DBG(<<"sfile_hdl_base_t::disable(" << hdl << ")");

	void	*owner = hdl._link.member_of();
	if (!owner)
		return;
	else if (owner != &_list) {
		cerr.form("sfile_handler_t(%#lx): handle %#lx doesn't belong to me!\n",
			  (long) this, (long) &hdl);
		return;
	}

	hdl.disable();
	hdl._link.detach();
	hdl._owner = 0;

#if 0
	/* XXX what if there is another event on the same descriptor ? */
	if (hdl.fd == dtbsz - 1)
		--dtbsz;
#endif
}


void sfile_handler_t::enable(sfile_hdl_base_t &hdl)
{
	if (hdl._mode & rd) {
		rc++;
		FD_SET(hdl.fd, &masks[0]);
	}
	if (hdl._mode & wr) {
		wc++;
		FD_SET(hdl.fd, &masks[1]);
	}
	if (hdl._mode & ex) {
		ec++;
		FD_SET(hdl.fd, &masks[2]);
	}
}


void sfile_handler_t::disable(sfile_hdl_base_t &hdl)
{
	if (hdl._mode & rd) {
		rc--;
		FD_CLR(hdl.fd, &masks[0]);
	}
	if (hdl._mode & wr) {
		wc--;
		FD_CLR(hdl.fd, &masks[1]);
	}
	if (hdl._mode & ex) {
		ec--;
		FD_CLR(hdl.fd, &masks[2]);
	}
}
	


/*
 *  sfile_handler_t::prepare(timeout)
 *
 *  Prepare the sfile to wait on file events for timeout milliseconds.
 */

w_rc_t sfile_handler_t::prepare(const stime_t &timeout, bool forever)
{
	if (forever)
		tvp = 0;
	else {
		tval = timeout;
		tvp = &tval;
	}

	fd_set *rset = rc ? &ready[0] : 0;
	fd_set *wset = wc ? &ready[1] : 0;
	fd_set *eset = ec ? &ready[2] : 0;

	if (rset || wset || eset)
		memcpy(ready, masks, sizeof(masks));
	w_assert1(dtbsz <= FD_SETSIZE);

#ifdef DEBUG
	if (gotenv > 1) {
		gotenv = 0;
		cerr << "select():" << endl;
		if (rset)
			cerr << "\tread_set:" << *rset << endl; 
		if (wset)
			cerr << "\twrite_set:" << *wset << endl;
		if (eset)
			cerr << "\texcept_set:" << *eset << endl;

		cerr << "\ttimeout= ";
		if (tvp)
			cerr << timeout;
		else
			cerr << "INDEFINITE";
		if (timeout==0)
			cerr << " (POLL)";
		cerr << endl << endl;
	}
	if (fd_allzero(rset) && fd_allzero(wset) && fd_allzero(eset)) {
		w_assert1(tvp!=0);
	}
#else
	w_assert1(rset || wset || eset || tvp);
#endif

	any[0] = rset;
	any[1] = wset;
	any[2] = eset;

	return RCOK;
}


/*
 *  sfile_handler_t::wait()
 *
 *  Wait for any file events or for interrupts to occur.
 */
w_rc_t sfile_handler_t::wait()
{
	fd_set *rset = any[0];
	fd_set *wset = any[1];
	fd_set *eset = any[2];

	SthreadStats.selects++;

	int n = select(dtbsz, rset, wset, eset, tvp);
	int select_errno = ::errno;
	
	switch (n)  {
	case -1:
		if (select_errno != EINTR)  {
			cerr << "select(): "<< ::strerror(select_errno) << endl;
			if (rset)
				cerr << "\tread_set:" << *rset << endl;
			if (wset)
				cerr << "\twrite_set:" << *wset << endl;
			if (eset)
				cerr << "\texcept_set:" << *eset << endl;
			cerr << endl;
		} else {
			// cerr << "EINTR " << endl;
			SthreadStats.eintrs++;
		}
		return RC(sthread_t::stOS);
		break;

	case 0:
		return RC(sthread_base_t::stTIMEOUT);
		break;

	default:
		SthreadStats.selfound++;
		break;
	}

	return RCOK;
}


/*
 *  sfile_handler_t::dispatch()
 *
 *  Dispatch select() events to individual file handles.
 *
 *  Events are processed in order of priority.  At current, only two
 *  priorities, "read"==0 and "write"==1 are supported, rather crudely
 */

void sfile_handler_t::dispatch()
{
	/* any events to dispatch? */
	if (!(any[0] || any[1] || any[2]))
		return;

	direction = (direction == 0) ? 1 : 0;
	
	sfile_hdl_base_t *p;

	w_list_i<sfile_hdl_base_t> i(_list, direction);

	bool active[3];

	while ((p = i.next()))  {
		/* an iterator across priority would do better */
		if (p->priority() != 0)
			continue;

		active[0] = any[0] && FD_ISSET(p->fd, ready+0);
		active[1] = any[1] && FD_ISSET(p->fd, ready+1);
		active[2] = any[2] && FD_ISSET(p->fd, ready+2);

		if (active[0] || active[1] || active[2])
			p->dispatch(active[0], active[1], active[2]);
	}
	
	for (i.reset(_list);  (p = i.next()); )  {
		if (p->priority() != 1)
			continue;

		active[0] = any[0] && FD_ISSET(p->fd, ready+0);
		active[1] = any[1] && FD_ISSET(p->fd, ready+1);
		active[2] = any[2] && FD_ISSET(p->fd, ready+2);

		if (active[0] || active[1] || active[2])
			p->dispatch(active[0], active[1], active[2]);
	}
}


/*
 *  sfile_handler_t::is_active(fd)
 *
 *  Return true if there is an active file handler for fd.
 */

bool sfile_handler_t::is_active(int fd)
{
	w_list_i<sfile_hdl_base_t> i(_list);
	sfile_hdl_base_t *p; 

	while ((p = i.next()))  {
		if (p->fd == fd)
			break;
	}

	return p != 0;
}


ostream &sfile_handler_t::print(ostream &o)
{
	w_list_i<sfile_hdl_base_t> i(_list);
	sfile_hdl_base_t* f = i.next();

	if (f)  {
		o << "waiting on FILE EVENTS:" << endl;
		do {
			f->print(o);
		} while ((f = i.next()));
	}

	return o;
}

ostream &operator<<(ostream &o, sfile_handler_t &h)
{
	return h.print(o);
}



/*
 *  sfile_hdl_base_t::sfile_hdl_base_t(fd, mask)
 */

sfile_hdl_base_t::sfile_hdl_base_t(int f, int m)
: fd(f),
  _mode(m),
  _enabled(false)
{
}    


/*
 *  sfile_hdl_base_t::~sfile_hdl_base_t()
 */

sfile_hdl_base_t::~sfile_hdl_base_t()
{
	if (_link.member_of()) {
		W_FORM(cerr)("sfile_hdl_base_t(%#lx): destructed in place (%#lx) !!!\n",
			     (long)this, (long)_link.member_of());
		/* XXX how about printing the handler ?? */
		/* XXX try removing it from the list? */
	}
}


/*
 * Allow changes to the file descriptor if the handle isn't in-use.
 */

w_rc_t	sfile_hdl_base_t::change(int new_fd)
{
	if (enabled() || running())
		return RC(sthread_t::stINUSE);
	fd = new_fd;
	return RCOK;
}


bool sfile_hdl_base_t::running() const
{
	return (_link.member_of() != 0);
}


bool sfile_hdl_base_t::enabled() const
{
	return _enabled;
}


void sfile_hdl_base_t::enable()
{
	if (enabled())
		return;
	if (!running())
		return;

	_owner->enable(*this);
	_enabled = true;
}


void sfile_hdl_base_t::disable()
{
	if (!enabled())
		return;
	if (!running())
		return;

	_owner->disable(*this);
	_enabled = false;
}

/* Revector this to sthread for compatability */
bool sfile_hdl_base_t::is_active(int fd)
{
	return sthread_t::io_active(fd);
}

/*
 *  sfile_hdl_base_t::dispatch()
 *
 *  Execute the appropriate callbacks for the sfile event.
 */

void sfile_hdl_base_t::dispatch(bool read, bool write, bool except)
{
    if (read && (_mode & rd))
	    read_ready();

    if (write && (_mode & wr))
	    write_ready();

    if (except && (_mode & ex))
	    exception_ready();
}


ostream &sfile_hdl_base_t::print(ostream &s) const
{
	s.form("sfile=%#lx [%sed] fd=%d mask=%s%s%s", 
	       (long)this,
	       enabled() ? "enabl" : "disabl",
	       fd,
	       (_mode & rd) ? " rd" : "",
	       (_mode & wr) ? " wr" : "",
	       (_mode & ex) ? " ex" : "");

#if 0
	if (enabled()) {
		s.form("ready[%s%s%s ]  masks[%s%s%s ]",
	       (_mode & rd) && FD_ISSET(fd, &ready[0]) ? " rd" : "",
	       (_mode & wr) && FD_ISSET(fd, &ready[1]) ? " wr" : "",
	       (_mode & ex) && FD_ISSET(fd, &ready[2]) ? " ex" : "",
	       (_mode & rd) && FD_ISSET(fd, &masks[0]) ? " rd" : "",
	       (_mode & wr) && FD_ISSET(fd, &masks[1]) ? " wr" : "",
	       (_mode & ex) && FD_ISSET(fd, &masks[2]) ? " ex" : ""
	       );
	}
#endif

	return s << '\n';
}

ostream &operator<<(ostream &o, sfile_hdl_base_t &h)
{
	return h.print(o);
}


sfile_safe_hdl_t::sfile_safe_hdl_t(int fd, int mode)
: sfile_hdl_base_t(fd, mode),
  _shutdown(false)
{
	char buf[20];
	ostrstream s(buf, sizeof(buf));
	s << fd << ends;
	sevsem.setname("sfile ", buf);
}


sfile_safe_hdl_t::~sfile_safe_hdl_t()
{
} 


w_rc_t 
sfile_safe_hdl_t::wait(long timeout)
{
	if (is_down())  {
		return RC(sthread_t::stBADFILEHDL);
	} 

	// XXX overloaded error code?
	if (!running())
		return RC(sthread_t::stBADFILEHDL);

	enable();

	W_DO( sevsem.wait(timeout) );

	if (is_down())
		return RC(sthread_t::stBADFILEHDL);

	W_IGNORE( sevsem.reset() );

	return RCOK;
}

/*
 *  sfile_safe_hdl_t::shutdown()
 *
 *  Shutdown this file handler. If a thread is waiting on it, 
 *  wake it up and tell it the bad news.
 */

void
sfile_safe_hdl_t::shutdown()
{
	_shutdown = true;
	W_IGNORE(sevsem.post());
}

void 
sfile_safe_hdl_t::read_ready()
{
	W_IGNORE(sevsem.post());
	disable();
}

void 
sfile_safe_hdl_t::write_ready()
{
	W_IGNORE(sevsem.post());
	disable();
}

void 
sfile_safe_hdl_t::exception_ready()
{
	W_IGNORE(sevsem.post());
	disable();
}



sfile_compat_hdl_t::sfile_compat_hdl_t(int fd, int mode)
: sfile_safe_hdl_t(fd, mode)
{
	w_assert3(fd >= 0);
	w_assert3(mode == rd || mode == wr || mode == ex);

	W_COERCE(sthread_t::io_start(*this));
}

sfile_compat_hdl_t::~sfile_compat_hdl_t()
{
	W_COERCE(sthread_t::io_finish(*this));
}
