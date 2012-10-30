/* --------------------------------------------------------------- */
/* -- Copyright (c) 1996 Computer Sciences Department,          -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
//
// $Id: oql_context.h,v 1.4 1996/07/26 00:17:55 nhall Exp $
//

#ifndef _OQL_CONTEXT_H_
#define	_OQL_CONTEXT_H_
/*
   A context used for compiling and executing
   OQL queries.
   Specifies a database that queries are executed/compiled
   against, and a set of 'defined' constructs.
 */

#include <types.h>
#include <typedb.h>
#include <scope.h>
#include <base_types.h>
#include <oql_tsi.h>
#include <tsi.h>

// 4/24/95 
// Add allocator support...
#include <lalloc.h>


class oqlContext;
class oqlDatabase;

class oqlDatabase {
   friend class oqlContext;
 private:
  char 			*_name;

public:
  oql_rc_t		AddBaseTypes();
  oql_rc_t 		AddADTs();
  oql_rc_t 		LoadAtomicTypes();

  oql_rc_t		LoadSdlModule();	// load user types
  oql_rc_t		LoadOtherTypes();	// load user types
  Type * 		AddShoreType(Ref<sdlType>);


public:
  oqlDatabase(const char* name);
  ~oqlDatabase();
  ExtentDB		extents;
  TypeDB		types;
  
  char*        		db_name() const { return _name; }
  oqlDatabase&		reset() {types.reset(); extents.reset();}
  
#ifndef STAND_ALONE
  //
  // remove type need to consider two cases:
  //	- all subclasses be removed?
  //	- all reference to this type are affected
  //
  oql_rc_t		remove_type(char *name);
  oql_rc_t		remove_type(Type *t);
#endif STAND_ALONE
};

class oqlContext {
  int    _open;               // Is a database open ?
  bool _tmpTypes;           // Are all types that are to be stored in the 
                              // database tmp types 
protected:
public:
  oqlContext(const char *name = "Its me");
  ~oqlContext();
  oqlDatabase	db;
  SymbolTable	defines;
  DataBase universe;
  Scope scope;


  oqlContext& setTransientTypes() {
     _tmpTypes = true; 
     return *this;
  }
  oqlContext& setEternalTypes()   {
     _tmpTypes = false; 
     return *this;
  }
  bool transientTypes()          {return _tmpTypes;}

  int open() {return _open;}
  oqlContext& reset(char* name);
  char* name() const { return db.db_name(); }


  oql_rc_t 		create_db(const char *db);
  oql_rc_t 		open_db(const char *db);
  oql_rc_t 		close_db();
  oql_rc_t 		drop_db(const char *db);
  oql_rc_t		drop_extent(const char* name);
  oql_rc_t		drop_index(const char* name);

};
extern oqlContext* oql_context();

#endif /* _OQL_CONTEXT_H_ */










