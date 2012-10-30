/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994,95,96 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef _STRACE_H_
#define _STRACE_H_

class _strace_t;
ostream &operator<<(ostream &s, const class _strace_t &);


class strace_t {
	enum {
		default_alloc_hint = 64,
	};

	static	long id_generator;	// unique event IDs
	int	_alloc_hint;		// allocation strategy hint

	/* the first element of each chunk of allocated memory
	   is retained so they can be deleted */
	_strace_t	*_heads;

	_strace_t	*_free;
	_strace_t	*_held;

	bool	more();
	void	_release(_strace_t *);	// XXX kludgomatic

public:
	strace_t(int hint = default_alloc_hint);
	~strace_t();

	void		push(const char *name,
			     const void *id,
			     bool isLatch = false);
	void		pop(const void *id);

	_strace_t	*get();
	_strace_t	*get(const char *name,
			     const void *id,
			     bool isLatch = false);
	void		release(_strace_t *);


	ostream		&print(ostream &) const;
};

ostream &operator<<(ostream &s, const class strace_t &);

#endif /* _STRACE_H_ */ 
