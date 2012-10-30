/* --------------------------------------------------------------- */
/* -- Copyright (c) 1996 Computer Sciences Department,          -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#include <iostream.h>
#include "Aglob_vars.h"
#include <types.h>
#include <Bparsestate.h>
Aglob_vars_t gvars;
class Aglob_vars_t *Aglob_vars () { return &gvars;};
//void Asearch_t::Cleanup (void){}
Aglob_vars_t *Aglob_vars ();
#include <oql_context.h>
oqlContext goql_c;
oqlContext* oql_context()
{ return &goql_c;}
int saveError(int, char *, int) { return 0;}
checkError(char *, char const *){return 0;}
int
missing_fct() { return 0;}
extern "C"
QPROC_ERROR_CHECK() {}

int lineno;
int yylval;
int yytext;
int Anumpages;

// a bunch of print stuff from pplan expr_t's.  unclear how paradise
// gets away without these?
#include <parameter.h>

// #include <Berrstream.h>
#include <oql_context.h>
// Berrstream_t dummy_erst; // (cerr);
// note: Bparsestate_t has a errstream but I don't know whats current.
ostream& errstream() {return cerr; }


#include <Alist.h>


// Bparser_state_t methods
void
Bparser_state_t::add_uniq_attr(Bastnode_t *)
{
	missing_fct();
}




// crummy parser conversions
class node *
v_to_node(struct Ql_tree_node *)
{
	missing_fct();
	return 0;
}
struct Ql_tree_node *
expr_to_v(Ref<sdlExprNode>)
{
	missing_fct();
	return 0;
}

int Do(Ref<sdlDeclaration>, Scope&)
{
	missing_fct();
	return 0;
}

// missing global setup functions.
int setupOpenDb(const char* name)
{
	missing_fct();
	return 0;
}

int setupCloseDb()
{
	missing_fct();
	return 0;
}
int PrimitiveType::isInteger()
{return _primitive == Sdl_long;}
#include <type_globals.h>
#include <sdl_ext.h>
void
set_decl(WRef<sdlDeclaration> dref, const char *name, Ref<sdlType> type, DeclKind kind, int lineno = 0, Zone z=Public);
// push an oql type out to the db as an sdl type.

Ref<sdlExprNode>
v_to_expr(class Ql_tree_node *){ missing_fct(); return 0;}

ErrorType::ErrorType(const char *name) : Type(name, ODL_Error) 
{	
	missing_fct();
}

