/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#include <stdio.h>
#include <assert.h>
#include <w.h>
#include "string_t.h"
#include <debug.h>


void
testit(
	const char *_path,
	const char *to_push
)
{
	bool		pushed=false;
	string_t	path(_path); //uses OwnSpace
	DBG(
		<< "svas_server::_lookup2 _path " << path 
	);
	while (!path.empty()) {
		DBG(<<"path= " << path );
		if(path.is_absolute()) {
			DBG(
				<< "is absolute:" << path
			)
			(void) path.pop(0); // should pop off any trailing '/'s also
			continue;
		} else {
			DBG( << "is relative: " << path)
			// skip prefixes ".[/]*"
			if(path.pop_prefix(".")) 
				continue;
			assert(!path.empty());
		}
		assert(!path.empty());

		assert (path.is_relative());

		// get the next _path part, null-terminated
		DBG(<<"path=" << path);
		path._strtok("/");
		assert(strlen(path.ptr()) > 0);
		/**/

		DBG(<<"path=" << path << " about to pop "
			<< strlen(path.ptr()) );
		path.pop(strlen(path.ptr()));
		DBG(<<"path=" << path << " after pop "); 
		assert(path.is_relative());

		if(!pushed) {
			if(!path.empty()) path.push("/");
			path.push((Path)to_push); // gets copied
			DBG(<<"path after pushed " << to_push << ":" << path ); 
			pushed = true;
		}
	}// while loop 

}

main()
{
	DBG(<< "************" << __LINE__ );
	testit("","x");

	DBG(<< "************" << __LINE__ );
	testit("/","x");

	DBG(<< "************" << __LINE__ );
	testit("/y234/z234678/q12","x");

	DBG(<< "************" << __LINE__ );
	testit("xrinf","y");

	DBG(<< "************" << __LINE__ );
	testit("y/xrinf","");

	DBG(<< "************" << __LINE__ );
	testit("y/xrinf","/x/y/z");

	DBG(<< "************" << __LINE__ );
	testit("y234/z234678/q12","x");
}
