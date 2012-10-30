/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: thread4.c,v 1.12 1996/10/16 00:07:38 bolo Exp $
 */
#include <iostream.h>
#include <iomanip.h>
#include <strstream.h>
#include <stdlib.h>
#include <time.h>
#include <memory.h>

#include <w.h>
#include <sthread.h>

class timer_thread_t : public sthread_t {
public:
	timer_thread_t(unsigned ms);

protected:
	virtual void run();

private:
	unsigned _ms;
};

unsigned default_timeout[] = { 
	4000, 5000, 1000, 6000, 4500, 4400, 4300, 4200, 4100, 9000
};

bool	verbose = false;


main(int argc, char **argv)
{
	int		timeouts;
	unsigned	*timeout;

	if (argc > 1 && strcmp(argv[1], "-v") == 0) {
		verbose = true;
		argc--;
		argv++;
	}
	if (argc > 1) {
		timeouts = argc - 1;
		timeout = new unsigned[timeouts];
		for (int i = 1; i < argc; i++)
			timeout[i-1] = atoi(argv[i]);
	}
	else {
		timeouts = sizeof(default_timeout) /sizeof(default_timeout[0]);
		timeout = default_timeout;
	}

	timer_thread_t **timer_thread = new timer_thread_t *[timeouts];

	int i;
	for (i = 0; i < timeouts; i++)  {
		timer_thread[i] = new timer_thread_t(timeout[i]);
		w_assert1(timer_thread[i]);
		W_COERCE(timer_thread[i]->fork());
	}

	for (i = 0; i < timeouts; i++)  {
		W_COERCE( timer_thread[i]->wait());
		delete timer_thread[i];
	}

	delete [] timer_thread;
	if (timeout != default_timeout)
		delete [] timeout;

	return 0;
}

    

timer_thread_t::timer_thread_t(unsigned ms)
    : sthread_t(t_regular, 0, 0, "timer_thread"), _ms(ms)
{
}

void timer_thread_t::run()
{
	stime_t	start, stop;

	cout << "[" << setw(2) << setfill('0') << id 
		<< "] going to sleep for " << _ms << "ms" << endl;

	if (verbose)
		start = stime_t::now();

	sthread_t::sleep(_ms);

	if (verbose)
		stop = stime_t::now();
	
	cout << "[" << setw(2) << setfill('0') << id 
		<< "] woke up after " << _ms << "ms";
	if (verbose)
		cout << "; measured " << (sinterval_t)(stop-start)
		<< " seconds.";
	cout << endl;
}

