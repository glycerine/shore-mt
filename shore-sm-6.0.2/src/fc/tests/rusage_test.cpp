/*<std-header orig-src='shore'>

 $Id: rusage_test.cpp,v 1.1 2010/07/19 18:35:06 nhall Exp $

SHORE -- Scalable Heterogeneous Object REpository

Copyright (c) 1994-99 Computer Sciences Department, University of
                      Wisconsin -- Madison
All Rights Reserved.

Permission to use, copy, modify and distribute this software and its
documentation is hereby granted, provided that both the copyright
notice and this permission notice appear in all copies of the
software, derivative works or modified versions, and any portions
thereof, and that both notices appear in supporting documentation.

THE AUTHORS AND THE COMPUTER SCIENCES DEPARTMENT OF THE UNIVERSITY
OF WISCONSIN - MADISON ALLOW FREE USE OF THIS SOFTWARE IN ITS
"AS IS" CONDITION, AND THEY DISCLAIM ANY LIABILITY OF ANY KIND
FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.

This software was developed with support by the Advanced Research
Project Agency, ARPA order number 018 (formerly 8230), monitored by
the U.S. Army Research Laboratory under contract DAAB07-91-C-Q518.
Further funding for this work was provided by DARPA through
Rome Research Laboratory Contract No. F30602-97-2-0247.

*/

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#ifdef _WINDOWS
#include <ctime>
#endif

#include <w.h>
#include <w_rusage.h>

double consume(int i, double f);

int
main()
{
    unix_stats U;
    U.start();

    double f= 3.14159;

	cout << "thinking..." << endl;
	const int iter=1000;
    for(int i=1; i <= iter; i++) {
		f += consume(i, f);
		cout << "i " << i << " f now  " << f << endl << flush;
    }

    U.stop(iter); // 1 iteration

    cout << "Unix stats :" <<endl;
    cout << "float f=" << f << endl;
    cout << U << endl << endl;
    cout << flush;

    return 0;
}

double consume(int i, double f)
{
	cout << " f in  " << f << endl << flush;
	(void) new char[i];
	while(--i)
	{
		f *= 1.11;
		f /= 1.1;
	}
	cout << " f out  " << f << endl << flush;
	return f;
}
