/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 * $Header: /p/shore/shore_cvs/src/vas/common/msg_stats.C,v 1.11 1995/04/24 19:44:24 zwilling Exp $
 */
#include "msg_stats.h"

ostream &operator << (ostream &o, const _msg_stats &m) 
{
	int j, q, last= m.first+m.nmsgs;
	const char **n = m._info->names;
	const int  *v	 = m._info->values;

	for( ; *v > 0;  v++, n++) {
		q = *v;
		// this flaky-looking "if" IS necessary-- just
		// because our awk script isn't smart enough to 
		// create 2 different sets of msg_names structs
		// for cmsgs and vmsgs.
		if(q > m.first && q < last) {
			j = m.offset(q);
			if(m._count[j]>0){
				o << "\t" << *n
#ifdef DEBUG
					<< "(" << q << ")"
#endif
				<< " " << m._count[j] << endl;
			}
		}
	}
	return o;
}


w_statistics_t &operator << (w_statistics_t &o, _msg_stats &m) 
{
	int j;
	const char **n   = m._info->names;
	const int  *v	 = m._info->values;

	// locate the offset of the first value in this batch
	for( ; *v > 0 && *v!=m.first;  v++, n++);

	if(*v == m.first) {
		j = m.offset(*v);
		// m._count[j] is where the counters start
		//
		// Use the version of add_module that treats
		//  the values as static
		const w_stat_t	*dummy = (const w_stat_t *)&m._count[j];
		w_rc_t e;
		e = o.add_module_static(
			m._descr, m._base, m.nmsgs, n, m._types, dummy);
		if(e) {
			cerr <<"Cannot add module " << m._descr << endl;
		}
	} else {
		cerr << "Cannot find first entry for " << m._descr
			<<"; first= " 
			<< m.first << " nmsgs=" << m.nmsgs 
			<< "; looking for " << *v
			<< endl;
	}
	return o;
}

#ifdef DEBUG

/* debugging stuff */
#include <stream.h>
#include <ostream.h>
#include "msg.h"
extern "C" void print_gather_reply(gather_stats_reply *r);

void
print_gather_reply(gather_stats_reply *r)
{
	stats_module        *m = r->modules.modules_val;
	int k;
	
	// DEBUGGING TOOL -- PRINTS TO cout
	k = r->modules.modules_len;
	cout <<  k << " modules" << endl;
	while(k-- > 0) {
		cout << "Module: " << m->descr.possibly_null_string_u.str << " at " 
			<< ::hex((unsigned int)m) << endl;
		cout << " \tbase  " << m->base << endl;
		cout << " \tcount  " << m->count << endl;
		cout << " \tlongest  " << m->longest << endl;
		cout << " \ttypes  " << m->types.possibly_null_string_u.str 
			<< " len=" << strlen(m->types.possibly_null_string_u.str)
			<< endl;
		cout << " \tmsgs_len  " << m->msgs.msgs_len << endl;
		cout << " \tfirst msg is "<<m->msgs.msgs_val[0];
		cout << " \tvalues_len  " << m->values.values_len << endl;
		cout << " \tfirst val is type "<< m->values.values_val[0].tag 
			<< " value is " << (int)m->values.values_val[0].stat_values_u._i
			<< endl;
		m++;
	}

}

#endif
