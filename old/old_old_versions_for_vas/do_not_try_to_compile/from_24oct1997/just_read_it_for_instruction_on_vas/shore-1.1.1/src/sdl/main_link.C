/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#include "sdl_internal.h"
#include "sdl_ext.h"
void metaobj_init(int argc, char *argv[]);
extern int sdl_errors;
extern int sdl_linking;
extern void UnSwizzleCache();
main(int argc, char *argv[])
{
	// init_sdl_db();
	// UnSwizzleCache();
	REF(Declaration) lpt;
	// linking phase: for now, just run through resolve_types again
	// with flag set.
	metaobj_init(argc,argv);
	sdl_linking = 1;
	W_COERCE(Shore::begin_transaction(3));
	REF(Module) mpt;

	for (int i = 1; i < argc; ++i)
	{
		W_COERCE(REF(Module)::lookup(argv[i], mpt));
		if (mpt==0)
		{
			fprintf(stderr,"couldn't find module %s\n",argv[i]);
			continue;
		}
		mpt.update()->resolve_types();
	}
	if (sdl_errors) // don't commit
	{
		fprintf(stderr,"%d error%s found; module(s) not committed\n",
			sdl_errors, sdl_errors==1 ? "" : "s");
		W_COERCE(Shore::abort_transaction());
	}
	else
		W_COERCE(Shore::commit_transaction());
}
