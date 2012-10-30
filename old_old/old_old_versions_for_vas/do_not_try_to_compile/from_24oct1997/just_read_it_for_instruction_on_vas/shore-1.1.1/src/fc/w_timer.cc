/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */


#ifdef __GNUC__
#pragma implementation

#if __GNUC__ >= 2 && __GNUC_MINOR__ > 6
/* gcc 2.7.2 has bogus warning messages; it doesn't inherit pointer
   properties correctly */
#define	GCC_IS_BOGUS
#endif

#endif

#define W_SOURCE

#include <w_base.h>

#include <stime.h>
#include <w_timer.h>

#ifdef GCC_IS_BOGUS
#define	GCC_PIBUG(type)	(type)
#else
#define	GCC_PIBUG(type)
#endif	


w_timer_t::w_timer_t(const stime_t &when)
: _next(this),
  _prev(this),
  _queue(0),
  _when(when),
  _fired(false)
{
}


w_timer_t::~w_timer_t()
{
	/* Special case: The w_timer_t in a w_timerqueue_t will belong
	   to itself.  We could check that (_queue == this), but that
	   depends upon _queue being the first element of the
	   timerqueue.  Instead of the explicit check, Just verify
	   that prev/next point to ourself */

	if (!(_next == this  && _prev == this)) {
		W_FORM(cerr)("w_timer_t(%#lx): destructed in eventQ(%#lx) !!!\n",
			  (long)this, (long)_queue);
	}
}

/* insert ourselves before 'at' */
void	w_timer_t::attach(w_timerqueue_t &to, w_timer_t &at)
{
	_queue = &to;
	_next = &at;
	_prev = at._prev;
	at._prev = this;
	_prev->_next = this;
}


void	w_timer_t::detach()
{
	w_assert3(_queue);
	_queue = 0;
	_prev->_next = _next;
	_next->_prev = _prev;
	_next = _prev = this;
}


void	w_timer_t::reset(const stime_t &when)
{
	w_assert1(_queue == 0);
	_when = when;
}


ostream	&w_timer_t::print(ostream &s) const
{
	W_FORM(s)("w_timer_t(%#lx) fire%s at ",
		  (long) this,
		  _fired ? "d" : "s");
	s << _when;
	return s;
}


w_timerqueue_t::w_timerqueue_t()
{
	_queue.attach(*this, _queue);
}


w_timerqueue_t::~w_timerqueue_t()
{
	w_timer_t	*event;

	if (isEmpty())
		return;

	W_FORM(cerr)("timerqueue_t(%#lx): destroy untriggered events ...\n",
		  (long)this);

	for (event = _queue._next; event != &_queue; event = _queue._next) { 
		event->detach();
		cerr << *event << endl;
		event->destroy();
	}
}
			  

bool	w_timerqueue_t::nextEvent(stime_t &when) const
{
	if (isEmpty())
		return false;
	
	when = _queue._next->_when;
	return true;
}


bool	w_timerqueue_t::isEmpty() const
{
	return _queue._next == GCC_PIBUG(w_timer_t *) &_queue;
}


stime_t	w_timerqueue_t::nextTime() const
{
	return isEmpty() ? stime_t::sec(0) : _queue._next->_when;
}


void	w_timerqueue_t::schedule(w_timer_t &event)
{
	w_timer_t	*at;

	/* Find insertion point, duplicates queued FIFO */
	for (at = _queue._next; at != &_queue; at = at->_next)
		if (at->_when > event._when)
			break;

	event.attach(*this, *at);
}


void	w_timerqueue_t::cancel(w_timer_t &event)
{
#if 1
	w_assert1(event._queue == this);
#else	
	if (event._queue != this)
		return;
#endif	

	event.detach();
}


void	w_timerqueue_t::run(const stime_t &now)
{
	w_timer_t	*event;

	for (event = _queue._next; event != &_queue; event = _queue._next) {
		if (event->_when > now)
			break;

		event->detach();
		/* XXX
		   What would be the best detach and _fired policy, 
		   before or after the event??
		   Must consider triggers which reschedule themselves,
		   and triggers that are never fired, and triggers
		   which destroy themselves when fired.
		 */
		event->_fired = true;
		event->trigger();
	}
}


ostream &w_timerqueue_t::print(ostream &s) const
{
	w_timer_t	*event;

	W_FORM(s)("w_timerqueue_t(%#lx):", (long) this);

	event = _queue._next;
	if (event == GCC_PIBUG(w_timer_t *)&_queue) {
		s << " empty";
		return s;
	}
	
	s << endl;

	for (; event != GCC_PIBUG(w_timer_t *)&_queue; event = event->_next)
		s << "\t" << *event << endl;

	return s;
}
