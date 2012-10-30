/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994,5, 6  Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#include <iostream.h>
#include <assert.h>
#include <stddef.h>

#include <w.h>

#include <w_timer.h>

w_timerqueue_t events;

class mytimer : public w_timer_t {
public:
	void	trigger() {
		cout << "mytimer ... " << *this << endl;
	}
};

mytimer	one, two, three, four, five;

// extern "C" unsigned long time(unsigned long *);

main()
{
	cout << events << endl;

	stime_t	now(stime_t::now());

	one.reset(now + (stime_t)10.0);
	two.reset(now + (stime_t)10.0);
	three.reset(now + (stime_t)15.0);
	four.reset(now + (stime_t)32.0);
	five.reset(now + (stime_t)93.0);
	
	events.schedule(one);
	events.schedule(two);
	events.schedule(three);
	events.schedule(four);
	events.schedule(five);

	cout << "it is now " << now << endl;
	cout << "it is now " << (double)now << ", double" << endl;
	cout << events << endl;

	stime_t	then;
	while (events.nextEvent(then)) {
		cout << "next in " << (sinterval_t)(then - now)
			<< ", sinterval_t";
		cout << " or " << (double)(then - now)
			<< ", double";
#if 0
		/* gcc 2.7.2 can't decide on a conversion */
		cout << " or " << (unsigned long)(then - now)
			<< ", unsigned long";
#endif
		cout << endl;

#ifdef stupid 
		while (now < then) {
			sleep(10);
			now = stime_t::now();
		}
#else
		sleep((int)(double)(then - now));
		now = then;
#endif

		cout << "Running events" << endl;
		events.run(now);
		cout << events << endl;
	}

	cout << "Finis" << endl << events << endl; 

	return 0;
}
