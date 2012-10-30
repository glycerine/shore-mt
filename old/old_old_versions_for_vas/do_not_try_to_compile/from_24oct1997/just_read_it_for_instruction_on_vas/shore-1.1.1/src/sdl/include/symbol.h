#ifndef _SYMBOL_H_
#define _SYMBOL_H_
/* --------------------------------------------------------------- */
/* -- Copyright (c) 1996 Computer Sciences Department,          -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#include <auto_string.h>
#include <lalloc.h>

class SymbolTable;
class Symbol {
friend ostream &operator<<(ostream &s, Symbol &sym);
friend class SymbolTable;
friend class SymbolTable_i;
private:
   auto_string	_name;
   const void	*_data;
   int	_flags;
   SymbolTable	*_table;
   
public:
   Symbol(const char *name,
	  const void *data = 0,
	  SymbolTable *table = (SymbolTable *)0) : _name(name) {
	     _data = data;
	     _table = table;
	     _flags = 0;
	  }
   ~Symbol() {
      _data = 0;
   }
#ifdef USE_ALLOCATORS
   Symbol(Allocator* ator, const char* name, 
	  void* data = 0, SymbolTable* table = 0) 
      : _name(ator, name), _data(data), _table(table), _flags(0) {}
#endif USE_ALLOCATORS
   
   const char	*name()  { return _name.str(); }
   const void	*data()	{ return _data; }
   SymbolTable	*table() { return _table; }
   void	setData(const void *data) { _data = data; }
   void	setTable(SymbolTable *table) { _table = table; }

   void	setFlags(int flags) { _flags = flags; }
   int	flags() { return _flags; }
   
   bool	matches(const char *name);

#ifdef USE_ALLOCATORS
   void* operator new(long sz);
   void* operator new(long sz, Allocator* ator);
   void  operator delete(void *) {}
#endif USE_ALLOCATORS
};

ostream &operator<<(ostream &s, Symbol &sym);

class SymbolTable {
friend	ostream &operator<<(ostream &s, SymbolTable &t);
friend class SymbolTable_i;
private:
   const	int	SIZE = 100;
   auto_string	_name;
   Symbol	*syms[SIZE];
   int	_size;

   int	_lookup(const char *name);
   void	_remove(int offset, int zap = 1);
   ostream	&print(ostream &s);

#ifdef USE_ALLOCATORS
   Allocator* _ator;
#endif USE_ALLOCATORS

public: /* protected: */
   Symbol	*iterate(int &token);
   Symbol* iterate(int& token, const char* name);
   Symbol	*_enter(const char *name, const void *data = 0);

public:
   SymbolTable(const char *name = "<noname>") : _name(name) {
      _size = 0;
      for (int i = 0; i < SIZE; i++)
	 syms[i] = 0;
#ifdef USE_ALLOCATORS
      _ator = DefaultAllocator();
#endif USE_ALLOCATORS
   }
#ifdef USE_ALLOCATORS
   SymbolTable(Allocator* ator, const char* name = "<noname>")
      : _name(ator, name), _size(0), _ator(ator) {
      for (int i = 0; i < SIZE; i++)
	 syms[i] = 0;
   }

   void* operator new(long sz);
   void  operator delete(void *) {}
#endif USE_ALLOCATORS

   ~SymbolTable() {reset();}

#ifdef USE_ALLOCATORS
   SymbolTable& setAllocator(Allocator* ator) {
      _ator = ator;
      return *this;
   }
#endif USE_ALLOCATORS   

   int  size() {return _size;}
   Symbol *overwrite(const char *name, const void *data = 0);
   Symbol *enter(const char *name, const void *data = 0, int overwrite = 0);
   Symbol *lookup(const char *name);
   int	remove(Symbol *sym);
   int	remove(const char *name);
   const char	*name() { return _name; }
   void reset();
};

class SymbolTable_i {
   int   _pos;
   const char* _name;
   SymbolTable	&_table;
 protected:
   void reposition();
 public:
   SymbolTable_i(SymbolTable& table)
      : _table(table), _pos(0), _name(0) {}
   SymbolTable_i(SymbolTable &table, const char* name)
      : _table(table), _pos(0), _name(name) {}

   SymbolTable_i& operator ++();
   Symbol* operator *();
   int eof();
   SymbolTable_i& rewind() {_pos = 0;}
};

ostream &operator<<(ostream &s, Symbol &s);
ostream &operator<<(ostream &s, SymbolTable &t);

#endif /* _SYMBOL_H_ */


