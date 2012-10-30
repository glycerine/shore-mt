#ifndef _TYPEDB_H_
#define _TYPEDB_H_
/* --------------------------------------------------------------- */
/* -- Copyright (c) 1996 Computer Sciences Department,          -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#include <symbol.h>
#include <types.h>

#include <lalloc.h>

class GenericDB : public SymbolTable {
public:
   GenericDB(const char *whatfor) : SymbolTable(whatfor) { }
#ifdef USE_ALLOCATORS
   GenericDB(Allocator* ator, const char* whatfor)
      : SymbolTable(ator, whatfor) {}
#endif USE_ALLOCATORS
   void add(Type *t);	              /* Add a named type */
   void add(const char *name, Type *t);     /* Add a name for a type */
   virtual Type *lookup(const char *name);
   virtual Type *force_lookup(const char *name);
   int  remove(const char* name);
};

class TypeDB : public GenericDB {
public:
   TypeDB(const char *name) : GenericDB(name) {}
#ifdef USE_ALLOCATORS
   TypeDB(Allocator* ator, const char* name) : GenericDB(ator, name) {}   
#endif USE_ALLOCATORS
       /* commit new types to the store */
   virtual Type *lookup(const char *name);
   virtual Type *force_lookup(const char *name);
   void	commit();
   void	save();
};

class ExtentDB : public GenericDB {
public:
   ExtentDB(const char *name) : GenericDB(name) {}
#ifdef USE_ALLOCATORS
   ExtentDB(Allocator* ator, const char* name) : GenericDB(ator, name) {}
#endif USE_ALLOCATORS
};



#endif /* _TYPEDB_H_ */ 
