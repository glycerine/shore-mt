/* --------------------------------------------------------------- */
/* -- Copyright (c) 1996 Computer Sciences Department,          -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#ifndef OQL_RC_H
#define OQL_RC_H

#define OQL_OK 		0

#ifndef _HAS_STDLIB_H_
#	define _HAS_STDLIB_H
#	include <stdlib.h>
#endif

#ifndef IOSTREAM_H
#	include <iostream.h>
#endif
#include <oql_error.h>
#include <oql_error.i>

typedef int oql_rc_t;

#define OQL_DO(x) {							 \
		      int rc = (x);					 \
		      if (rc != OQL_OK) {				 \
			cerr << "Error: " << oql_errmsg[rc-oqlERRMIN]	 \
			  << __FILE__ << " line "			 \
			  << __LINE__ << "\n";				 \
			return rc;					 \
		      }     						 \
		    }

#define OQL_COERCE(x) {						  \
			  int rc = (x);					  \
			  if ((x) != OQL_OK) {				  \
			    cerr << "Error: " << oql_errmsg[rc-oqlERRMIN] \
			      << __FILE__ << " line "			  \
				<< __LINE__ << "\n";			  \
			    abort();					  \
			  }						  \
			}

#define OQL_FATAL(x) {						  \
			 cerr << "Error: " << oql_errmsg[(x)-oqlERRMIN] \
			   << __FILE__ << " line " << __LINE__		  \
			     << "\n";					  \
			 abort();					  \
		       }


#ifndef DEBUG
#	define oql_assert(x) assert(x)
#else
#	define oql_assert(x)
#endif

inline
void w_warn(const char*	msg,
	    const char* fname,
	    const int	lineno)
{
  cerr << "WARNING: " << msg << " in " << fname << " on line "
    << lineno << endl;
}

inline
void w_err(const char*	msg,
	    const char* fname,
	    const int	lineno)
{
  cerr << "ERROR: " << msg << " in " << fname << " on line "
    << lineno << endl;
  abort();
}

#endif

