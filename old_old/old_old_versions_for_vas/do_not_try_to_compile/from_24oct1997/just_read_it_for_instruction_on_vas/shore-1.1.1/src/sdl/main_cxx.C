/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#include "sdl_internal.h"
#include "sdl_ext.h"
void metaobj_init(int argc, char *argv[]);


int doing_metatypes = 0; // flag for metatype special casing
main(int argc, char *argv[])
{
	REF(Declaration) lpt;
	// C++ language binding goes in n pases.
	// first pass: print out ref definitions for all interfaces
	// 2nd pass: print c++ classes
	// 3rd pass: print out compiled-in type support.
	// first, traverse module list, printing out language
	// binding proper (e.g. the header file portion.
	// ok, pass 1:
	printf("#include \"ShoreApp.h\"\n");
	metaobj_init(argc,argv);
	W_COERCE(Shore::begin_transaction(3));
	REF(Module) mpt;

	if (argc >1){
		 W_COERCE(REF(Module)::lookup(argv[1], mpt));
		 if (!mpt.type_ok())
		 {
			fprintf(stderr,"registered object %s not a type module\n",argv[1]);
			exit(1);
		}
	}

	//for (lpt = *g_module_list; lpt != 0; lpt=lpt->next)
	if (mpt != 0)
		mpt->print_cxx_binding();
	W_COERCE(Shore::commit_transaction());
}

