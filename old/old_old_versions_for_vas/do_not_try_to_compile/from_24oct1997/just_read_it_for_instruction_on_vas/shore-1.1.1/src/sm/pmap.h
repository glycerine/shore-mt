/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: pmap.h,v 1.1 1997/05/19 20:21:44 nhall Exp $
 */
#ifndef PMAP_H
#define PMAP_H

#ifdef __GNUG__
#pragma interface
#endif

#ifndef BITMAP_H
#include <bitmap.h>
#endif

struct Pmap {
	/* number of bits */
	enum	{ _count = smlevel_0::ext_sz };
	/* number of bytes */
	enum	{ _size = smlevel_0::ext_map_sz_in_bytes };

	u_char bits[_size];

	inline	Pmap() {
		clear_all();
	}

	inline	void	set(int bit) { bm_set(bits, bit); }
	inline	void	clear(int bit) { bm_clr(bits, bit); }

	inline	bool	is_set(int bit) const { return bm_is_set(bits, bit); }
	inline	bool	is_clear(int bit) const { return bm_is_clr(bits, bit);}

	inline	int	num_set() const { return bm_num_set(bits, _count); }
	inline	int	num_clear() const { return bm_num_clr(bits, _count); }

	inline	int	first_set(int start) const {
		return bm_first_set(bits, _count, start);
	}
	inline	int	first_clear(int start) const {
		return bm_first_clr(bits, _count, start);
	}
	inline	int	last_set(int start) const {
		return bm_last_set(bits, _count, start);
	}
	inline	int	last_clear(int start) const {
		return bm_last_clr(bits, _count, start);
	}

	inline	unsigned	size() const { return _size; }
	inline	unsigned	count() const { return _count; }

#ifdef notyet
	inline	bool	is_empty() const { return (num_set() == 0); } 
#else
	/* bm_num_set is too expensive for this use.
	 XXX doesn't work if #bits != #bytes * 8 */
	inline	bool	is_empty() const {
		unsigned	i;
		for (i = 0; i < _size; i++)
			if (bits[i])
				break;
		return (i == _size);
	}
#endif
	inline	void	clear_all() { bm_zero(bits, _count); }
	inline	void	set_all() { bm_fill(bits, _count); }

	ostream	&print(ostream &s) const;
};

extern	ostream &operator<<(ostream &, const Pmap &pmap);

/* Aligned Pmaps... Depending upon the pmap size it automagically
   provides a filler in the pmap to align it to a 2 byte boundary.
   This aligned version is used in various structures to guarantee
   size and alignment of other members */

#if (((SM_EXTENTSIZE+7) & 0x8) == 0)
typedef	Pmap	Pmap_Align2;
#else
class Pmap_Align2 : public Pmap {
public:
	inline	Pmap_Align2	&operator=(const Pmap &from) {
		*(Pmap *)this = from;	// don't copy the filler
		return *this;
	}
private:
	fill1	filler;		// keep purify happy
};
#endif


#endif /* PMAP_H */
