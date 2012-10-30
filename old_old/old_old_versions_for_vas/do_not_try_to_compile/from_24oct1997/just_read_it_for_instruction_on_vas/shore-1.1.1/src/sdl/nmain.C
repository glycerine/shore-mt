/* --------------------------------------------------------------- */
/* -- Copyright (c) 1996 Computer Sciences Department,          -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#include <fstream.h>
#include <assert.h>
#include <stdlib.h>
#include <iomanip.h>

#ifndef STAND_ALONE
#include <Aglob_vars.h>
//#include <Aopdefs.h>

//#include <Asearch.h>
//#include <Ahash.h>

#include <macros.h>
#include <lalloc.h>
#endif STAND_ALONE

#include <metatypes.sdl.h>
#include <Bparsestate.h>
#include <m_list.h>

#include <lalloc.h>
#include <oql_context.h>

#include <defns.h>

#ifndef STAND_ALONE
#include <globals.h>

// makes sure that mutex is released
// when returning from function.
class Aauto_mutex_t {
private:
  int flag;			// true if mutex has been acquired
#ifdef NO_SDL
  smutex_t &mutex;
public:
  Aauto_mutex_t (smutex_t &m) : mutex (m) {flag = 0;}
  ~Aauto_mutex_t (void) {if (flag) mutex.release ();}
  w_rc_t acquire (void) {flag = 1; return mutex.acquire ();}
  void release (void) {flag = 0; mutex.release ();}
#else
// do nothing
public:
  w_rc_t acquire (void) {flag = 1; return RCOK;}
  void release (void) {flag = 0; }
#endif

};

#ifdef NO_SDL
smutex_t scanAndParse("Sql Scanner and Parser");
#endif
#endif STAND_ALONE


class Declaration;
class Ql_tree_node;

int oql_parser();
int OqlExecuteQuery(Ql_tree_node *, oqlContext &);
//int OqlExecuteTypedecl(m_list_t<Ref<sdlDeclaration> > *, oqlContext &);
int OqlExecuteTypedecl(Ref<sdlDeclaration> , oqlContext &);

#ifdef STAND_ALONE
extern oqlContext* oql_context;
#endif STAND_ALONE

void
Bparser_state_t::Init(istream *input)
{
   Reset(); 
   _line = 1;
   _eof = 0;

   // Take care of mem_allocation here...
   if (_in)
   {
      delete _in; _in = NULL ;
   }
   _in = input;


   _scanner->yyrestart(_in);
}
	
char * errorString;
int do_one_query (const char *query_str, char *result_file)
{
   int error_code;



  // Set the result filename to NULL
  // Only queries need to have a result
   // Bparser_state()->Init((char *)query_str, "");
   // treat as a file name...
   ifstream *q_stream;
   q_stream = new ifstream(query_str);
   Bparser_state()->Init(q_stream);
while (!q_stream->eof())
{
	


  // Do the parsing...
   if (oql_parser()== 0)
   {
      return 0;
   }

	// if in sdl mode finish any decl processing...
	extern Ref<sdlDeclaration> g_module_list;
	Ref<sdlDeclaration> lpt;
	extern int sdl_errors;
	for (lpt = (g_module_list);
		lpt != 0;
		lpt = (lpt->next))
	{
		if (lpt->kind==Mod)
		{
		 ((Ref<sdlModDecl> &)lpt)->dmodule.update()->resolve_types();
			break;
		}
		// some other decl-- scope issues are problematic here.
		//if (lpt->scope==0) 
		//	lpt.update()->scope = this;
		// bpt->resolve_names(this,0);
		lpt->resolve_names(0,0); // dubious
		// if (lpt->scope==0)
		// 	lpt.update()->scope = this;
		if (lpt->kind==ExternType) continue;
		if (lpt->type != 0 && lpt->type->size == 0)
		{
			if (lpt->type->tag==Sdl_struct&& 
				((Ref<sdlStructType> &)(lpt->type))->members==0)
				continue;
			lpt->type.update()->compute_size();
			if (lpt->type->size==0) //  && sdl_linking
			{
				lpt->print_error_msg("unresolved reference in type; size not known");
			}
		}
	}
	if (sdl_errors) // stop now
	{
		fprintf(stderr,"%d error%s found; module(s) not created\n",
			sdl_errors, sdl_errors==1 ? "" : "s");
		  return paraQ_PARSE_ERROR;
	}





	// Check if there is a command at all
   if (!Bparser_state()->Query() && !Bparser_state()->Decls())
      return paraNOERROR;

	// There is a request... Execute it.
   if (Bparser_state()->Query() != NULL)
   {
     // All types created in this session are transient types
     // They will not be visible in the next incarnation of the server...
      oql_context()->setTransientTypes();
      error_code = OqlExecuteQuery(Bparser_state()->Query(), *oql_context());
      // if (error_code == paraNOERROR)
      // strcpy (result_file, Bparser_state()->resultFilename());
   }
   else 
   {
	// It is a type declaration...
     // All types declared in this session will remain in the database...
      oql_context()->setEternalTypes();
      error_code = OqlExecuteTypedecl(Bparser_state()->Decls(),
				      *oql_context());
   }

   if (error_code != paraNOERROR)
   {
      return error_code;
   }
}

   // Aglob_vars()->search->CleanUp ();
   return paraNOERROR;
}

int Aexecute_query (const char *query_str, char *result_file)
{

   int num_queries = 1;

  // setting up the stuff

   Bparser_state_t parser_state;
   
   Aglob_vars()->parser_state = &parser_state;

   if (getenv("DONT_EXECUTE"))
      Aglob_vars()->dont_execute_queries = 1;
   if (getenv("PRINT_PLAN"))
      Aglob_vars()->print_plan = 1;
   if (getenv("NO_PPLAN"))
      Aglob_vars()->make_pplan = 0;

   int error_code = do_one_query (query_str, result_file);
  
   return error_code;
}



