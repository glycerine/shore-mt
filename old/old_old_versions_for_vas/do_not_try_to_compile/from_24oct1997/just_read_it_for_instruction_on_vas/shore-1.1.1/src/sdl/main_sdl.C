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

extern yyparse();

extern int scheck_only ;
extern int lineno; //  in lex.yy.C
int checkout_flag = 0;
extern decl_pt  g_module_list;
// this is a copy of module::list_append, which allocates out of the
// global collecton instead of the module-specific collection.
int process_input()
{
	REF(ModDecl)  lpt;
	Ref<Declaration> dpt;
	// dump_str_tab();

	if (!scheck_only)
		W_COERCE(Shore::begin_transaction(3));
	lineno = 1;
	if (yyparse())
		return -1;
	if (scheck_only)
		return 0;
	if (g_module_list)
	for (lpt = (REF(ModDecl) &)(g_module_list);
		    lpt != 0;
		    lpt = (REF(ModDecl) &)(lpt->next))
		lpt->dmodule.update()->resolve_types();
	if (sdl_errors) // don't commit
	{
		fprintf(stderr,"%d error%s found; module(s) not created\n",
			sdl_errors, sdl_errors==1 ? "" : "s");
		shrc rc ;
		rc = Shore::abort_transaction();
		if (rc)
			; // always has an rc. du.
	}
	else
		SH_DO(SH_COMMIT_TRANSACTION);
		//W_COERCE(Shore::commit_transaction());
	return sdl_errors;
}

#ifdef needmain
int main(int argc, char *argv[])
{
	if (argc>1) scheck_only = 1; //yuck //yuck
	if (!scheck_only)
		metaobj_init(argc,argv);
	insert_rwords();
	// should check for options, but we don't have andy
	process_input();
}
#endif
