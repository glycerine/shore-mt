/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#include "ShoreApp.h"
#include "metatypes.sdl.h"
#include "type_globals.h"


shrc
get_type_oid(const char *mpath,
	char  *iname,
	LOID		&result)
{
	shrc e;
	Ref<sdlModule> m;

	m = lookup_module(mpath);
	if(m!= 0) {
		Ref<sdlType> t = m->resolve_typename(iname);
		if(t!= 0) {
			e = t.get_primary_loid(result);
		} else {
			e = RC(SH_NotFound);
		}
	} else {
		e = RC(SH_NotFound);
	}
	return e;
}

#ifdef notdef
//// test the function
main(int argc, char **argv)
{
	LOID 	oid;
	shrc	e;

	SH_DO(Shore::init(argc, argv));
	SH_DO(Shore::begin_transaction());

	if(e=get_type_oid("/types/PScan","Part", oid)) {
		cerr << e << endl;
	} else {
		cerr << "found oid=" << oid << endl;
	}
	SH_DO(Shore::commit_transaction());
}
#endif
