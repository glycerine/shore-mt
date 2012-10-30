/* --------------------------------------------------------------- */
/* -- Copyright (c) 1996 Computer Sciences Department,          -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#include <scope.h>
#include <tree.h>

int TypeScope::push(ScopeType* t)
{
   if (_next == MAX_SCOPE_SIZE)
      return 0;
   _tscope[_next++] = t;
   return 1;
}

int TypeScope::pop()
{
   if (_next == 0)
      return 0;
   --_next;
   return 1;
}

int TypeScope::RemoveType(const char* name)
{
   if (_next == 0)
      return 0;
   return _tscope[_next - 1]->RemoveType(name);
}

int TypeScope::AddType(const char* name, Type* t)
{
   if (_next == 0)
      return 0;
   _tscope[_next - 1]->AddType(name, t);
}

Type* TypeScope::lookup(ScopeType* t, const char* name)
   // Looks to see if name is defined in the immediate scope of t
{
   return t? t->lookup(name): 0;
}

/**
  Kludges for now
  I dont attempt to resolve scoped_names
**/
Type* TypeScope::ResolveTypename(ScopedName& name)
{
   return ResolveTypename(name.bottom()->id());
}

Type* TypeScope::ImmediateResolveTypename(ScopedName& name)
{
   return ImmediateResolveTypename(name.bottom()->id());
}
/**
  End kludge
**/

Type* TypeScope::ImmediateResolveTypename(const char* name)
{
   return lookup(_tscope[_next - 1], name);
}

Type* TypeScope::ResolveTypename(const char* name)
{
   Type* t;
   for (int i = _next - 1; i >= 0; i--)
      if (t = lookup(_tscope[i], name))
	 return t;
   return 0;
}

/** Container stuff begins here **/
int ContainerScope::push(ContainerType* t)
{
   if (_next == MAX_SCOPE_SIZE)
      return 0;
   _cscope[_next++] = t;
   return 1;
}

int ContainerScope::pop()
{
   if (_next == 0) 
      return 0;
   _next--;
   return 1;
}

Type* ContainerScope::isMember(const char* name)
{
   if (_next == 0)
      return 0;
   return _cscope[_next - 1]->member(name);
}

Type* ContainerScope::isImmediateMember(const char* name)
{
   if (_next == 0)
      return 0;
   return _cscope[_next - 1]->immediate_member(name);
}

// 
int ContainerScope::ContainsMe(ContainerType* t)
{
   for (int i = _next - 1; i >= 0; i--)
      if (_cscope[i] == t)
	 return 1;
   return 0;
}

