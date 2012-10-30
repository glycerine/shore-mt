/* --------------------------------------------------------------- */
/* -- Copyright (c) 1996 Computer Sciences Department,          -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#include <stdlib.h>
#include <ostream.h>
#include <string.h>
#include <stddef.h>

#include <lalloc.h>
#include <symbol.h>

#ifdef USE_ALLOCATORS
void* Symbol::operator new(long sz)
{
   return DefaultAllocator()->alloc(sz);
}

void* SymbolTable::operator new(long sz)
{
   return DefaultAllocator()->alloc(sz);
}

void* Symbol::operator new(long sz, Allocator* ator)
{
	return ator->alloc(sz);
}
#endif USE_ALLOCATORS

bool	Symbol::matches(const char *name)
{
   return strcmp(name, _name) == 0;
}

void SymbolTable::reset()
{
   int	i;

   for (i = 0; i < _size; i++) {
      delete syms[i];
      syms[i] = (Symbol *)0;
   }
   _size = 0;
}

int SymbolTable::_lookup(const char *name)
{
   int	i;

   for (i = _size-1; i >= 0 ; i--) {
      if (syms[i]->matches(name))
	 return i;
   }
   return -1;
}

ostream &SymbolTable::print(ostream &s)
{
   int	i;

   s << "SymbolTable(" << _name
      << ") (" << _size << " entries) {\n";
   for (i = 0; i < _size; i++)
      s << '\t' << *syms[i] << '\n';
   return s << "}\n";
}

Symbol *SymbolTable::lookup(const char *name)
{
   int	offset;

   offset = _lookup(name);
   return (offset == -1) ? ((Symbol *)0) : syms[offset];
}

Symbol *SymbolTable::_enter(const char *name, const void *data)
{
   Symbol	*s;

   if (_size >= SIZE) {
      cerr << "symbol table " << name << " out of space\n";
      return (Symbol *)0;
   }
#ifdef USE_ALLOCATORS
   s = new (_ator) Symbol(_ator, name, data, this);
#else  USE_ALLOCATORS
   s = new Symbol(name, data, this);
#endif USE_ALLOCATORS
   syms[_size++] = s;
   return s;
}

Symbol	*SymbolTable::overwrite(const char *name, const void *data)
{
   Symbol	*s;

   s = lookup(name);
   if (s) {
      s->setData(data);
      return s;
   }
   return _enter(name, data);
}


Symbol *SymbolTable::enter(const char *name, const void *data, int do_overwrite)
{
   return do_overwrite ? overwrite(name, data) : _enter(name, data);
}

void SymbolTable::_remove(int offset, int zap)
{
   int	i;

   if (zap)
      delete syms[offset];
   for (i = offset; i < _size-1; i++)
      syms[i] = syms[i+1];
   _size--;
   syms[_size] = (Symbol *)0;
}

int SymbolTable::remove(Symbol *sym)
{
   int	offset;

   offset = _lookup(sym->name());
   if (offset == -1  ||  syms[offset] != sym)
      return -1;

   _remove(offset, 0);
   return 0;
}

int SymbolTable::remove(const char *name)
{
   int offset = _lookup(name);

   if (offset == -1)
      return -1;
   _remove(offset, 1);
   return 0;
}

Symbol* SymbolTable::iterate(int& token)
{
   Symbol* result;
   if (token < 0 || token >= _size)
      return (Symbol *)0;
   result = syms[token++];
   if (token >= _size)		/* give clue of last entry */
      token = -1;
   return result;
}

// Assumes that name is non-NULL, and non-empty
Symbol *SymbolTable::iterate(int &token, const char* name)
{
   // I'm assuming that this is always true
   //   if (!name || !name[0])
   //      return iterate(token);
   Symbol *result;
   while (token >= 0 && token < _size)
   {
      if (!strcmp(syms[token]->name(), name)) break;
      token++;
   }
   if (token < 0 || token >= _size)
      return (Symbol *)0;
   result = syms[token++];
   if (token >= _size)		/* give clue of last entry */
      token = -1;
   return result;
}

ostream &operator<<(ostream &s, Symbol &sym)
{
   return s << "<" << sym._name << ", " << int(sym._data) << ">";
}

ostream &operator<<(ostream &s, SymbolTable &table)
{
   return table.print(s);
}

void SymbolTable_i::reposition()
{
   if (!_name) 
      return;
   if (eof()) 
      return;
   while (_pos < _table._size && strcmp(_name, _table.syms[_pos]->name()))
      _pos++;
   return;
}

SymbolTable_i& SymbolTable_i::operator ++()
{
   if (eof())
      return *this;
   _pos++;
   reposition();
   return *this;
}

Symbol* SymbolTable_i::operator *()
{
   if (eof())
      return 0;
   return _table.syms[_pos];
}

int SymbolTable_i::eof()
{
   return (_pos >= _table._size);
}


	  

