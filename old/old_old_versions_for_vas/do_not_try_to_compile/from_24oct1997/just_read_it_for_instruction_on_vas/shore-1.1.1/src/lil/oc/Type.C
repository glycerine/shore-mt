/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

//
// Type.C
//

#ifdef __GNUG__
#pragma implementation "Type.h"
#pragma implementation "OCTypes.h"
#endif

#include "Type.h"

#ifdef oldcode
rType::rType(rType ** p, char * n, int nind)
: base_list(p)
{
    if(n != 0){
		int len = strlen(n);
		char *s = new char[len + 1];
		strcpy(s, n);
		name = s;
    }
    nindices = nind;
	size = 0;
	next = 0;
}
#endif
