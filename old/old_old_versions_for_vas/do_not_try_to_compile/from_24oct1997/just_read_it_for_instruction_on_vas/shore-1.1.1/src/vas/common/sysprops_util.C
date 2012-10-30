/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */


#include <externc.h>
#include <msg.h>
#include "sysprops.h"
#include "xdrmem.h"
#include <svas_error_def.h>
#include <debug.h>

#define HAS_MASK 	0x0f000000
#define HAS_INDEXES 0x01000000
#define HAS_TEXT 	0x02000000


enum ObjectKind	
ptag2kind(
	sysp_tag t, 
	bool *has_text, // =0 
	bool *has_indexes
)
{
	enum ObjectKind k;
	unsigned int rest;

	k = (ObjectKind) (t & ~HAS_MASK);
	rest = t & HAS_MASK;
	if(has_indexes) *has_indexes = ((rest & HAS_INDEXES)==HAS_INDEXES);
	if(has_text)  *has_text = ((rest & HAS_TEXT)==HAS_TEXT);
	return k;
}

sysp_tag	
kind2ptag(
	enum ObjectKind k, 
	bool has_text, 
	bool has_indexes
)
{
	sysp_tag t = (unsigned int)k;
	if(has_text)
		t |= HAS_TEXT;
	if(has_indexes)
		t |= HAS_INDEXES;
	return t;
}

sysp_tag	
kind2ptag(
	enum ObjectKind k, 
	ObjectOffset    tstart, 
	int	nindexes
)
{
	sysp_tag t = (unsigned int)k;
	if(tstart != NoText)
		t |= HAS_TEXT;
	if(nindexes > 0)
		t |= HAS_INDEXES;
	return t;
}

ObjectKind	
sysp_split(
	union _sysprops 	&s, 
	AnonProps			**_a, 
	RegProps			**_r,
	ObjectOffset		*_tstart,
	int					*_nindex,
	int					*_sz
)
{
	bool	 has_text, has_indexes;
	AnonProps			*ap;
	RegProps			*rp;
	int					szofsysprops=0;
	int					nindex=0;
	ObjectOffset 		tstart = NoText;
	ObjectKind			tag;

	tag = ptag2kind(s.common.tag, &has_text, &has_indexes);

	if(has_text) {
		if(has_indexes) {
			tstart 		= s.commontxtidx.text.tstart;
			nindex 		= s.commontxtidx.idx.nindex;
			ap			= &s.anontxtidx.anonprops;
			rp			= &s.regtxtidx.regprops;

			// not complete
			szofsysprops = 
				sizeof(_common_sysprops_withtextandindex);
		} else {
			tstart 		= s.commontxt.text.tstart;
			nindex 		= 0;
			ap			= &s.anontxt.anonprops;
			rp			= &s.regtxt.regprops;

			// not complete
			szofsysprops = 
				sizeof(_common_sysprops_withtext);
		}
	} else {
		tstart 		= NoText;
		if(has_indexes) {
			nindex 		= s.commonidx.idx.nindex;
			ap			= &s.anonidx.anonprops;
			rp			= &s.regidx.regprops;

			// not complete
			szofsysprops = 
				sizeof(_common_sysprops_withindex);
		} else {
			nindex 		= 0;
			ap			= &s.anon.anonprops;
			rp			= &s.reg.regprops;

			// not complete
			szofsysprops = 
				sizeof(_common_sysprops);
		}
	}
	switch(tag) {
		case KindRegistered:
			szofsysprops += sizeof(RegProps);
			break;

		case KindAnonymous:
			szofsysprops += sizeof(AnonProps);
			break;

		default: break;
	}
	dassert((unsigned int)(s.common.type._low) & 0x1);

	dassert(
		(szofsysprops == sizeof(struct _reg_sysprops)) ||
		(szofsysprops == sizeof(struct _reg_sysprops_withtext)) ||
		(szofsysprops == sizeof(struct _reg_sysprops_withindex)) ||
		(szofsysprops == sizeof(struct _reg_sysprops_withtextandindex)) ||
		(szofsysprops == sizeof(struct _anon_sysprops)) ||
		(szofsysprops == sizeof(struct _anon_sysprops_withtext)) ||
		(szofsysprops == sizeof(struct _anon_sysprops_withindex)) ||
		(szofsysprops == sizeof(struct _anon_sysprops_withtextandindex)) 
	);

	if(_a)	*_a = ap;
	if(_r)	*_r = rp;
	if(_tstart)	*_tstart = tstart;
	if(_nindex)	*_nindex = nindex;
	if(_sz) *_sz = szofsysprops;
	return tag;
}

static xdr_kind how(
	const union _sysprops *s
)
{
	xdr_kind xk;
	bool has_text, has_indexes;
	switch(ptag2kind(s->common.tag, &has_text, &has_indexes)) {
		case KindAnonymous:
			if(has_text) {
				if(has_indexes) {
					xk =  x_anon_sysprops_withtextandindex;
				} else {
					xk =  x_anon_sysprops_withtext;
				}
			} else {
				if(has_indexes) {
					xk =  x_anon_sysprops_withindex;
				} else {
					xk =  x_anon_sysprops;
				}
			}
			break;
		case KindRegistered:
			if(has_text) {
				if(has_indexes) {
					xk =  x_reg_sysprops_withtextandindex;
				} else {
					xk =  x_reg_sysprops_withtext;
				}
			} else {
				if(has_indexes) {
					xk =  x_reg_sysprops_withindex;
				} else {
					xk =  x_reg_sysprops;
				}
			}
			break;

		default:
			assert(0);
			break;
	}
	return xk;
}

int	
sysp_swap(const void *d, union _sysprops *s) // from disk to mem
{
	xdr_kind xk;
	if( disk2mem(s, d, x_common_sysprops)== 0 ) {
		return SVAS_XdrError;
	} 
	xk = how(s);
	if( disk2mem(s, d, xk)== 0 ) {
		return SVAS_XdrError;
	}
	return 0;
}

int	
sysp_swap(void *d, const union _sysprops *s) // from mem to disk
{
	xdr_kind xk;

	if( mem2disk(s, d, x_common_sysprops)== 0 ) {
		return SVAS_XdrError;
	}

	xk = how(s);

	if( mem2disk(s, d, xk)== 0 ) {
		return SVAS_XdrError;
	} 
	return 0;
}
