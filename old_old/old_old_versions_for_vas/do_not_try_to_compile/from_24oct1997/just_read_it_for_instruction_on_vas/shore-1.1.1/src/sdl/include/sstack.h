#ifndef _SSTACK_H_
#define _SSTACK_H_
/* --------------------------------------------------------------- */
/* -- Copyright (c) 1996 Computer Sciences Department,          -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */


class SymbolStackItem {
	friend class SymbolStack;
	friend class SymbolStack_i;
private:
protected:
	SymbolStackItem	*_next;
public:
	SymbolTable	_table;
	SymbolStackItem(const char *name) : _table(name) {
		_next = (SymbolStackItem *)0;
	}
#ifdef USE_ALLOCATORS
	void* operator new(long sz);
	void  operator delete(void *) {}
#endif USE_ALLOCATORS
};

class SymbolStack {
   friend class SymbolStack_i;
private:
	auto_string	_name;
	SymbolStackItem *_next;
public:
	SymbolStack(const char *name = "<noname>") : _name(name) {
		_next = (SymbolStackItem *)0;
	}
	~SymbolStack() {
		if (_next)
#ifdef DEBUG
			errstream() << "Symbol stack " << _name.str()
				<< " not empty at destructor!" << endl;
#endif DEBUG
			;
	}

	
	void push(const char *name) {
		SymbolStackItem *it = new SymbolStackItem(name);
		it->_next = _next;
		_next = it;
	}
#if 0
	SymbolTable *top() {
		return _next ? _next->_table : (SymbolTable *) 0;
	}
#endif
	void pop() {
		SymbolStackItem *it = _next;
		_next = it->_next;
		delete it;
	}
	bool	empty() { return _next == (SymbolStackItem *)0; }
	SymbolTable &top() {
#ifdef DEBUG
		if (empty())
			errstream() << "SymbolStack(" << _name.str() << ") empty!\n";
#endif DEBUG
		return _next->_table;
	}
	Symbol *lookup_all(const char *name) {
		SymbolStackItem *it = _next;
		Symbol *sym;
		while (it) {
			sym = it->_table.lookup(name);
			if (sym)
				return sym;
			it = it->_next;
		}
		return (Symbol *)0;
	}
	Symbol *lookup(const char *name) {
		return top().lookup(name);
	}
	Symbol *enter(const char *name, const char *data = 0, int overwrite = 0) {
		return top().enter(name, data, overwrite);
	}
	Symbol	*overwrite(const char *name, const char *data = 0) {
		return top().overwrite(name, data);
	}
	int	remove(const char *name) {
		return top().remove(name);
	}
#ifdef USE_ALLOCATORS
	void* operator new(long sz);
	void  operator delete(void *) {}
#endif USE_ALLOCATORS

};

class SymbolStack_i
{
 private:
   SymbolStackItem* _curr;
 public:
   SymbolStack_i(SymbolStack& s): _curr(s._next) {}
   SymbolTable* operator *() {return _curr? &_curr->_table: 0;}
   SymbolStack_i& operator ++() {
      if (_curr) _curr = _curr->_next;
      return *this;
   }
};

#endif /* _SSTACK_H_ */
