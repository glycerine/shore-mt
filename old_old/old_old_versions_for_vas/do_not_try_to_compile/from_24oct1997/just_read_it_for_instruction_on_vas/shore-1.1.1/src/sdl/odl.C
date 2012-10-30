/* --------------------------------------------------------------- */
/* -- Copyright (c) 1996 Computer Sciences Department,          -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#include "metatypes.sdl.h"
#include <defns.h>
#include <m_list.h>
#include <metatypes.sdl.h>

//#include <metatypes.c>
//#include <parse_support.c>
//#include <scope.c>
#include <oql_context.h>

#ifndef STAND_ALONE
//#include <pthread.h>
#include <globals.h>
#endif STAND_ALONE


//int OqlExecuteTypedecl(m_list_t<Ref<sdlDeclaration> > d_list, oqlContext& context)
int OqlExecuteTypedecl(Ref<sdlDeclaration>  decls_i, oqlContext& context)
{
   // m_list_i<Ref<sdlDeclaration> > decls_i(d_list);
   Ref<sdlDeclaration> d;

#ifndef STAND_ALONE
   // Is a database open? If none is open, just return, coz I cant
   //  define types in empty space
   if (!context.open())
   {
      errstream() << "No database is open currently. Open one first\n";
      return paraQ_ERROR;
   }
#endif STAND_ALONE
   // for (; d = *decls_i; decls_i++)
   for ( d = decls_i; d!=0; d = d->next)
   {
	  context.db.AddShoreType(d->type);
   }
   return paraNOERROR;
}

   
