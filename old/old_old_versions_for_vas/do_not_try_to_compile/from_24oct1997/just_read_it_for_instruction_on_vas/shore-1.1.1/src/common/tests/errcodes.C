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
main()
{
	cout << "ERROR CODES:\n"; 
	(void) w_error_t::print(cout);

}
option_group_t t(2); // causes error codes for options to
	// be included.

#ifdef __GNUC__
typedef w_auto_delete_array_t<char> gcc_kludge_1;
typedef w_list_i<option_t> 			gcc_kludge_0;

#endif /* __GNUC__*/

