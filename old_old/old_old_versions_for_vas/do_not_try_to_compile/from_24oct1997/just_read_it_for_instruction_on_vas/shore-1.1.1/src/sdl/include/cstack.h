/* --------------------------------------------------------------- */
/* -- Copyright (c) 1996 Computer Sciences Department,          -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
/*
  Context stack for oql -> aqua tranlations

 */
#ifndef _CSTACK_H_
#define _CSTACK_H_

class ContextStackItem {
	friend class ContextStack;
private:
protected:
	ContextStackItem	*_next;
public:
	aqua_op	_element;
	aqua_op	_combiner;
	aqua_t	*_predicate_extractor;
	aqua_t	*_var;

	int	cardinality;	/* # of items in context */
	int	at;		/* which item is being processed */
	
	ContextStackItem(aqua_op combine = a_nil, aqua_op item = a_nil) {
		_combiner = combine;
		_element = item;
		_predicate_extractor = (aqua_t *)0;
		_var = (aqua_t *)0;
		_next = (ContextStackItem *)0;
	}
#ifdef USE_ALLOCATORS
	void* operator new(long sz);
	void  operator delete(void *) {}
#endif USE_ALLOCATORS
};

class ContextStack {
private:
	auto_string	_name;
	ContextStackItem *_next;
public:
	ContextStack(char *name = "<noname>") : _name(name) {
		_next = (ContextStackItem *)0;
	}
	~ContextStack() {
	}

	void push(aqua_op combiner, aqua_op item) {
		ContextStackItem *it = new ContextStackItem(combiner, item);
		it->_next = _next;
		_next = it;
	}
	void pop() {
		ContextStackItem *it = _next;
		_next = it->_next;
		delete it;
	}
	bool	empty() { return _next == (ContextStackItem *)0; }
	// could have better error checking here
	ContextStackItem &top() {
		if (empty())
#ifndef STAND_ALONE
			errstream() << "ContextStack(" << _name.str()
				<< ") empty!\n"; 
#else
			;
#endif
		return *_next;
	}
	aqua_op	element() { return _next ? _next->_element : a_nil; }
	aqua_op	combiner() { return _next ? _next->_combiner : a_nil; }
	/* loose object problem with having elements have the error thing? */
	aqua_t	*PE() {
		if (_next && _next->_predicate_extractor)
			return _next->_predicate_extractor;
		else
			return new aqua_zary_t(a_nil);	/* aqua ERROR pls */
	}
	void	setPE(aqua_t *pe) {
		if (_next)
			_next->_predicate_extractor = pe;
	}
	aqua_t	*var() {
		if (_next && _next->_var)
			return _next->_var;
		else
			return new aqua_zary_t(a_nil);	/* aqua ERROR pls */
	}
	void	setVar(aqua_t *var) {
		if (_next)
			_next->_var = var;
	}

#ifdef USE_ALLOCATORS
	void* operator new(long sz);
	void  operator delete(void *) {}
#endif USE_ALLOCATORS
		
				
};

#endif /* _CSTACK_H_ */

