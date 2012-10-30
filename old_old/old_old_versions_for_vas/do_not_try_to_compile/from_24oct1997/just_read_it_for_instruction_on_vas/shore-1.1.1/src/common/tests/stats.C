/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <stream.h>
#include <iostream.h>
#include <stddef.h>
#include <limits.h>
#include "sm_app.h"
#include "option.h"
#include <debug.h>

#include "w_statistics.h"
#include "test_stat.h"

void statstest();

main()
{

	DBG(<<"app_test: main");
	statstest();
}

#ifdef __GNUC__
typedef w_auto_delete_array_t<char> gcc_kludge_1;
typedef w_list_i<option_t> 			gcc_kludge_0;
#endif /* __GNUC__*/


void
statstest()
{
	/*********w_statistics_t ***********/

	class w_statistics_t ST; // generic stats class
	class test_stat TST; // my test class that uses stats
	w_rc_t	e;

	// server side: implementor of RPC request does this:
	TST.compute();

	ST << TST;
	ST << TST;
	ST << TST;

	// then ST gets sent across the wire and re-instantiated
	// on client side

	// Various ways the contents of ST can be printed:
	{
		cout << "ST.printf():" << endl;
		ST.printf();
		cout << endl;

		cout << "cout << ST:" << endl;
		cout << ST;
		cout << endl;

		/*  
		 * Format some output my own way.
		 * This is a way to do it by calling
		 * on the w_statistics_t structure to provide
		 * the string name.
		 */
		cout << "My own pretty formatting for module : " << ST.module(TEST_i) << endl;
		W_FORM(cout)("\t%-30.30s %10.10d", 
				ST.string(TEST_i), ST.int_val(TEST_i)) 
			<< endl;
		W_FORM(cout)("\t%-30.30s %10.10d", 
				ST.string(TEST_j), ST.uint_val(TEST_j)) 
			<< endl;
		W_FORM(cout)("\t%-30.30s %10.6f", 
				ST.string(TEST_k), ST.float_val(TEST_k)) 
			<< endl;
		W_FORM(cout)("\t%-30.30s %10.10d", 
				ST.string(TEST_l), ST.float_val(TEST_l)) 
			<< endl;
		W_FORM(cout)("\t%-30.30s %10.10d", 
				ST.string(TEST_v), ST.float_val(TEST_v)) 
			<< endl;
		cout << endl;

		/*
		 * Error cases:
		 */
		cout << "Expect some unknowns-- these are error cases:" <<endl;
		cout <<  ST.typechar(TEST_i) << ST.typestring(TEST_STATMIN) << endl;
		cout <<  ST.typechar(-1,1) << ST.typestring(-1) 
				<< ST.string(-1,-1)<<  ST.float_val(-1,-1)
		<< endl;
	}
	{
		w_statistics_t *firstp = ST.copy_brief();
		w_statistics_t &first = *firstp;

		if(!firstp) {
			cout << "Could not copy_brief ST." << endl;
		
		} else {
			const w_statistics_t &CUR = ST;

			cout << __LINE__ <<  " :******* CUR.copy_brief(true): " << endl;
			first.printf();
			cout << endl;

			cout << __LINE__ << endl;
			TST.inc();
			cout << __LINE__<< " :******* TST.inc(): " << endl;
			CUR.printf();
			cout << endl;
		}
		{
			w_statistics_t *lastp = ST.copy_all();
			w_statistics_t &last = *lastp;

			if(!lastp) {
				cout << "Could not copy_all ST." << endl;
			
			} else {
				cout << __LINE__ << endl;

				cout << __LINE__ << endl;
				last -= first;
				cout << __LINE__ 
					<< " :******* last -= first: :" << endl;
				cout << last << endl;
				cout << endl;

				cout << __LINE__ << endl;
				last += first;
				cout << __LINE__ 
					<< " :******* last += first: :" << endl;
				cout << last << endl;
				cout << endl;

				cout << __LINE__ << endl;
				last.zero();
				cout << __LINE__ 
					<< " :******* last.zero(): :" << endl;
				cout << last << endl;
				cout << endl;

				cout << "Expect error:" << endl;

				ST -= first;
			}
			delete lastp;
		}
		delete firstp;
	}
	/********** end w_statistics_t tests ********/
}

