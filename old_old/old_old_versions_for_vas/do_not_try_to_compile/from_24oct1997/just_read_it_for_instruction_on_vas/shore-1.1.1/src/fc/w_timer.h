/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */


#ifndef W_TIMER_H
#define W_TIMER_H

#include <stime.h>

#ifdef __GNUG__
#pragma interface
#endif



class w_timerqueue_t;

class w_timer_t {
    friend class w_timerqueue_t;
private:
	w_timer_t	*_next;
	w_timer_t	*_prev;
	w_timerqueue_t	*_queue;

	stime_t		_when;
	bool		_fired;

	// function to execute when the event is triggered
	virtual void	trigger() = 0;

	// function to execute if the event is discarded.
	virtual void	destroy() { }

	// {,de}install me! 
	void	attach(w_timerqueue_t &to, w_timer_t &after);
	void	detach();

public:
	w_timer_t(const stime_t &when = stime_t::sec(0));
	virtual ~w_timer_t();

	/* play it again, sam */
	void	reset(const stime_t &when);

	const stime_t	&when() const {
		return _when;
	}

	bool	fired() const { return _fired; }

	virtual	ostream	&print(ostream &s) const;

private:
	// disabled
	w_timer_t(const w_timer_t &);
	w_timer_t	&operator=(const w_timer_t &);
};


class w_null_timer_t : public w_timer_t {
private:
	void	trigger() { }
	void	destroy() { }

public:
	w_null_timer_t() : w_timer_t(stime_t::sec(0)) { }
};


class w_timerqueue_t {
private:
	w_null_timer_t	_queue;

public:
	w_timerqueue_t();
	~w_timerqueue_t();

	// Combination atomic !isEmpty() + nextTime() 
	bool	nextEvent(stime_t &when) const;

	// any events ?
	bool	isEmpty() const;

	// time of next event, 0 if none
	stime_t	nextTime() const;

	// it is 'now'.  Run any timers which have expired.
	void	run(const stime_t &now);
	
	// schedule and cancel events
	void	schedule(w_timer_t &event);
	void	cancel(w_timer_t &event);

	ostream	&print(ostream &) const;

private:
	// disabled
	w_timerqueue_t(const w_timerqueue_t &);
	w_timerqueue_t	&operator=(const w_timerqueue_t &);
};


inline ostream &operator<<(ostream &s, const w_timer_t &t)
{
	return t.print(s);
}


inline ostream &operator<<(ostream &s, const w_timerqueue_t &q)
{
	return q.print(s);
}


#endif /* W_TIMER_H */
