/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994,95,96 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
/*
   New Style Resource Tracing ...

   This resource trace has a couple of advantages over the older code ...
   1) arbitrary trace sizes without needing to recomple the whole
      systems (just sthreads can be recompiled)
   2) resources Don't need to be nested in a LIFO order.  The old
      tracer only worked with lock-0 lock-1 unlock-1 unlock-0.  This
      new version will correctly tradce lock-0 lock-1 unlock-0
      unlock-1.

   As tracing is the primary facility for determining problems in
   a multi-threaded system, I want to increase the flexibility
   of this system by creating a "trace state" which can be
   pushed/popped from the trace stack.  Currently, the state
   is a combination of a "name" and an id (typically the pointer
   to the object, or some other unique id).  

   The only things currently traced are mutex's and latches.  The
   'isLatch' member is used to distinguish un-named mutexs from
   un-named latches.  This is an example of something which could be
   added to the trace state.

   XXX ***  Danger  Danger  Danger *** XXX
   The resource tracer currently maintains a "event id" so that the
   ordering of traced events OVER ALL THREADS may be maintained.
   It's not guarded by a mutex, since it's part of the resource tracing
   (which traces mutexs).   On a system with real threads, the "event id"
   must use cheap timestamps or an atomically interlocked instruction
   to generate event ids.  Otherwise heisenberg happens, and
   tracing will interfere with the natural order of execution.
 */

#include <iostream.h>

#include "w.h"
#include "w_error.h"
#include "w_rc.h"

#include "strace.h"

class _strace_t {
public:
	const	char *name;
	const	void *id;
	_strace_t	 *next;	// previous event
	unsigned long	event_id;
	bool	isLatch;

	_strace_t(_strace_t *_next = 0) {
		name = 0;
		id = 0;
		next = _next;
		event_id = 0;
		isLatch = false;
	}

	ostream	&print(ostream &) const;
};


long	strace_t::id_generator = 0;


strace_t::strace_t(int hint)
: _alloc_hint(hint),
  _heads(0),
  _free(0),
  _held(0)
{
	if (_alloc_hint < 1)
		_alloc_hint = default_alloc_hint;

	/* make space for list-head bookkeeping */
	_alloc_hint++;

	if (!more())
		W_FATAL(fcOUTOFMEMORY);
}


strace_t::~strace_t()
{
	_strace_t	*trace;

	_free = 0;
	_held = 0;

	while (_heads) {
		trace = _heads;
		_heads = trace->next;
		delete [] trace;
	}
}


inline _strace_t *strace_t::get()
{
	_strace_t	*trace;

	if (!_free && !more())
		return 0;

	trace = _free;
	_free = trace->next;
	return trace;
}


_strace_t *strace_t::get(const char *name, const void *id, bool isLatch)
{
	_strace_t *trace = get();

	if (!trace)
		return 0;

	trace->id = id;
	trace->name = name;
	trace->isLatch = isLatch;
	trace->event_id = id_generator++;

	return trace;
}


inline void	strace_t::_release(_strace_t *trace)
{
	trace->next = _free;
	_free = trace;
}


/* XXX this kludge because I don't know of a good way to both inline
   and expose this function, without _strace_t is exposed */

void	strace_t::release(_strace_t *trace)
{
	_release(trace);
}




bool	strace_t::more()
{
	int	i;
	_strace_t *trace;

	trace = new _strace_t[_alloc_hint];
	if (!trace)
		return false;

	/* initialize the chain and link it in */
	for (i = 0; i < _alloc_hint-1; i++)
		(void) new (trace + i) _strace_t(trace + i + 1);

	(void) new (trace + _alloc_hint - 1) _strace_t(_free);
	_free = trace;

	/* keep the chain head for book-keeping */
	// trace = _get();
	_free = trace->next;
	trace->next = _heads;
	_heads = trace;

	return true;
}


void	strace_t::push(const char *name, const void *id, bool isLatch)
{
	_strace_t	*trace;

	if (!(trace = get()))
		W_FATAL(fcOUTOFMEMORY);

	trace->name = name;
	trace->id = id;
	trace->event_id = id_generator++;
	trace->isLatch = isLatch;

	trace->next = _held;
	_held = trace;
}

#ifdef LIFO_ONLY
bool	arbitrary_release_ok = false;
#else
bool	arbitrary_release_ok = true;
#endif

void	strace_t::pop(const void *id)
{
	if (_held == 0) { 
		cerr.form("XXX resource pop @%#x invalid, continue\n", id);
		return;
	}

	/* optimize the common case -- LIFO */
	_strace_t *trace = _held;
	if (trace->id == id) {
		_held = trace->next;
		_release(trace);
		return;
	}
	if (!arbitrary_release_ok) {
	    cerr << "WARNING - freeing out of order! ID=" << (long)id  << endl;
	    cerr << "STACK=" << endl;
	    trace->print(cerr);
	}

	/* slow search otw */
	_strace_t *follow = trace;
	trace = trace->next;

	while (trace && trace->id != id) {
		follow = trace;
		trace = trace->next;
	}

	if (!trace) {
		cerr.form("XXX resource pop @%#x not found!!\n", id);
		return;
	}

	follow->next = trace->next;

	// choke if not allowing non-lifo activity
	if(! arbitrary_release_ok) {
		_strace_t *f = trace;
		while (f ) {
			f->print(cerr);
			f = f->next;
		}
	}
	w_assert3(arbitrary_release_ok);

	_release(trace);
}


ostream &operator<<(ostream &s, const _strace_t &trace)
{
	return trace.print(s);
}


ostream &_strace_t::print(ostream &s) const
{
	return s.form("%s @ %#lx  event=%ld%s\n",
		      name,
		      (long)id,
		      event_id,
		      isLatch ? " (latch)" : "");
}


ostream &operator<<(ostream &s, const strace_t &trace)
{
	return trace.print(s);
}

ostream &strace_t::print(ostream &s) const
{
	if (!_held)
		return s << "--no resources held-- " << endl;

	s << "resources held: (last .. first)" << endl;
	_strace_t *trace = _held;
	for (; trace; trace = trace->next)
		trace->print(s);

	return s;
}
