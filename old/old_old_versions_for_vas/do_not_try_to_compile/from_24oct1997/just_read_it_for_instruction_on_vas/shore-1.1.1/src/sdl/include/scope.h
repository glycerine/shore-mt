/* --------------------------------------------------------------- */
/* -- Copyright (c) 1996 Computer Sciences Department,          -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#ifndef	SCOPE_H
#define	SCOPE_H

#include <assert.h>
#include <defns.h>
#include <types.h>
#include <typedb.h>

const int MAX_SCOPE_SIZE = 16;

class TypeScope
{
   friend class Scope;
 protected:
   ScopeType* _tscope[MAX_SCOPE_SIZE];
   int  _next;

   TypeScope& reset() {_next = 0; return *this;}
   Type* lookup(ScopeType* s, const char* name);
 public:
   TypeScope() : _next(0) {}
//   ~TypeScope() {}

   ScopeType* top() {
      return (_next > 0) ? _tscope[_next - 1] : (ScopeType *)0;
   }

   Type* ResolveTypename(ScopedName& name);
   Type* ResolveTypename(ScopedName* name) {
      return ResolveTypename(*name);
   }
   Type* ResolveTypename(const char* name);

   Type* ImmediateResolveTypename(ScopedName& name);
   Type* ImmediateResolveTypename(ScopedName* name) {
      return ResolveTypename(*name);
   }
   Type* ImmediateResolveTypename(const char* name);

   int AddType(const char* name, Type* t);
   int RemoveType(const char* name);

   int pop();
   int push(Type* t) {
      return t->isScope()? push(t->isScope()): 0;
   }
   int push(ScopeType* t);
   
   int empty() { return (_next == 0); }
};

class ContainerScope
{
   friend class Scope;
 protected:
   ContainerType* _cscope[MAX_SCOPE_SIZE];
   int _next;

   ContainerScope& reset() {_next = 0; return *this;}
 public:
   ContainerScope() : _next(0) {}
//   ~ContainerScope() {}

   int pop();
   int push(ContainerType* t);
   int push(Type* t) {
      return t->isContainer()? push(t->isContainer()): 0;
   }
   
   ContainerType* top() {
      return (_next > 0) ? _cscope[_next - 1] : 0;
   }

   int empty() {return (_next == 0);}

   Type* isMember(const char* name);
   Type* isImmediateMember(const char* name);

   int ContainsMe(ContainerType* t);
   int ContainsMe(Type* t) {
      return t->isContainer()? ContainsMe(t->isContainer()): 0;
   }
};

class Scope
{
 protected:
   DataBase* _universe;
 public:
   void reset(DataBase* universe) {
      _universe = universe;
      tscope.reset();
      cscope.reset();
   }
   DataBase* universe() {return _universe;}
   TypeScope tscope;
   ContainerScope cscope;
};

#endif SCOPE_H
