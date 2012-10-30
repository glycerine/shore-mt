/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#include <stdio.h>
#include <stdlib.h>
#include <tcl.h>

#include "debug.h"
#define XDR void

#include "svas_client.h"

main()
{
#define S(x) cout << #x << ": " << sizeof(x) << endl;
	S(vas);
}
