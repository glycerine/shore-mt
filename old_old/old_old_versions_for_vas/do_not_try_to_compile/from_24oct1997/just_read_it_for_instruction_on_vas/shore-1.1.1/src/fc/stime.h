/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994,5, 6  Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#ifndef _STIME_H
#define	_STIME_H

extern "C" {
#include <sys/time.h>
}

/*
  "Shore Time"

  This class represents time.  It is designed for the thread package,
  but it attempts to be a general purpose time and interval class.

  Stime_t is an class that lets you build an underlying implementation
  which can utilize whatever the "best" (what a metric!) time
  information on a system is.  As a general philosophy, it must
  gracefully implement both short intervals, and absolute timestamps.
  Negative times should also work.  Currently there is some doubt
  about that, as none of the existing code which was converted to use
  stime_t, or my new code depend on negative time.  I haven't bothered
  testing and fixing it for those reasons.
  
  The current implementation, uses either BSD 'struct timeval' or
  Posix 'struct timespec' to represent both intervals and timestamps.
  Future implementors are encouraged to use something similar if other
  high resolution timers are available.  The combination of a
  "time-of-day" portion of time, and a "high resolution" between
  time-of-day increments works well for both purposes.

  I created the sinterval_t class for one purpose.  It's used as a
  cast so that you can subtract two times and print the result as an
  interval rather than an absolute date.

  A Note about time.  I deemed that automagically generate different
  kinds of time was too heavy-weight for the use of this package.  If
  you want something complicated like that ... TAKE IT SOMEPLACE ELSE.
  It doesn't belong here.  All this stuff HAS TO BE LIGHTWEIGHT.
  Thank you.  bolo, 20 December, 1995.
 */


class stime_t {
protected:
#ifdef USE_POSIX_TIME
	struct	timespec	_time;
#else
	struct	timeval		_time;
#endif

	/* better method name, PLEASE */
	void	gettime();

#ifdef __STIME_TESTER__
	/* Expose internals for the test program to cheat with */ 
public:
#endif
	void signs();
	void _normalize();
	void normalize();

	/* only good INSIDE, since workings exposed if public */
	stime_t(time_t, long);		/* time-of-day, hr-secs */

public:
	stime_t() {
		_time.tv_sec = 0;
#ifdef USE_POSIX_TIME
		_time.tv_nsec = 0;
#else
		_time.tv_usec = 0;
#endif
	}

	stime_t(const struct timeval &tv);
#ifdef USE_POSIX_TIME
	stime_t(const struct timespec &ts);
#endif

	// an interval in floating point seconds
	stime_t(double);

	/* comparison primitives */
	int	operator==(const stime_t &) const;
	int	operator<(const stime_t &) const;
	int	operator<=(const stime_t &) const;
	/* derived compares */
	int	operator!=(const stime_t &r) const { return !(*this == r); }
	int	operator>(const stime_t &r) const { return !(*this <= r); }
	int	operator>=(const stime_t &r) const { return !(*this < r); }

	// negate an interval
	stime_t	operator-() const;

	/* times can be added and subtracted */
	stime_t	operator+(const stime_t &r) const;
	stime_t	operator-(const stime_t &r) const;

	/* adjust an interval by a factor ... an experiment in progress */
	/* XXX should this be confined to sinterval_t ??? */ 
	stime_t operator*(const int factor) const;
	stime_t operator/(const int factor) const;
	stime_t operator*(const double factor) const;
	stime_t operator/(const double factor) const;

	/* XXX need a rounding operator?? */ 

	/* output conversions.  int/long is not available,
	   because it doesn't have the dynamic range */
	operator double() const;
	operator float() const;
	operator struct timeval() const;
#ifdef USE_POSIX_TIME
	operator struct timespec() const;
#endif
	
	/* XXX really belongs in sinterval_t */
	/* simple output conversions for integers to eliminate fp */
	long	secs();			/* seconds */
	long	msecs();		/* mill seconds */
	long	usecs();		/* micro seconds */
	long	nsecs();		/* nano seconds */

	/* input conversion operators for integral types */
	static	stime_t sec(int seconds); 
	static	stime_t	usec(int micro_seconds, int seconds = 0);
	static	stime_t	msec(int milli_seconds, int seconds = 0);
	static	stime_t	nsec(int nano_seconds, int seconds = 0);

	/* the Current time */
	static	stime_t now();

	ostream	&print(ostream &s) const;
	/* XXX notyet a printForm() method which allows time to
	   be formatted the way you want */
};


/* Intervals are different, in some ways, from absolute
   times.  For now, they are to change print methods */

class sinterval_t : public stime_t {
public:
	/* XXX why do I duplicate the constructors ???  There
	 is or was a reason for it. */

	sinterval_t() : stime_t() { }
	sinterval_t(const struct timeval &tv) : stime_t(tv) { }
#ifdef USE_POSIX_TIME
	sinterval_t(const struct timespec &ts) : stime_t(ts) { }
#endif
	sinterval_t(const stime_t &time) : stime_t(time) { }
	sinterval_t(double time) : stime_t(time) { }

	ostream	&print(ostream &s) const;
};


extern ostream &operator<<(ostream &s, const stime_t &t);
extern ostream &operator<<(ostream &s, const sinterval_t &t);

#endif /* _STIME_H */
