/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: shell.h,v 1.12 1997/05/19 19:49:17 nhall Exp $
 *
 * Everything common to shell.c, shell2.c, etc
 */

#include <limits.h>
#ifndef W_H
#include "w.h"
#endif
#ifndef SM_VAS_H
#include "sm_vas.h"
#endif
#ifndef __DEBUG_H__
#include "debug.h"
#endif
#ifndef SSH_H
#include "ssh.h"
#endif
#ifndef SSH_RANDOM_H
#include "ssh_random.h"
#endif
#include <tcl.h>
#include <string.h>
#include "ssh_error.h"
#include "tcl_thread.h"

#ifndef NBOX_H
#include <nbox.h>
#endif

#ifdef USE_COORD
#include <sm_coord.h>
#include <sm_global_deadlock.h>
extern sm_coordinator* co;
extern CentralizedGlobalDeadlockServer *globalDeadlockServer;
#endif


#define SSH_VERBOSE 

#if !defined(SOLARIS2) && !defined(HPUX8)
extern "C" {
	extern	int	strcasecmp(const char *, const char *);	
}
#endif

extern ss_m* sm;

extern bool start_client_support; // from ssh.c

typedef int smproc_t(Tcl_Interp* ip, int ac, char* av[]);
const MAXRECLEN = 1000;


typedef w_base_t::uint1_t  uint1_t;
typedef w_base_t::uint2_t  uint2_t;
typedef w_base_t::uint4_t  uint4_t;
typedef w_base_t::int1_t  int1_t;
typedef w_base_t::int2_t  int2_t;
typedef w_base_t::int4_t  int4_t;
typedef w_base_t::f4_t  f4_t;
typedef w_base_t::f8_t  f8_t;

struct typed_value {
    int _length;
    union {
	char *  bv;
	char    b1;
	uint4_t u4_num;
	int4_t  i4_num;
	uint2_t u2_num;
	int2_t  i2_num;
	uint1_t u1_num;
	int1_t  i1_num;
	f4_t    f4_num;
	f8_t    f8_num;
    } _u; 
};

static char   outbuf[(ss_m::page_sz * 2) > (1024 * 16) ? 
		    (ss_m::page_sz * 2) : (1024 * 16) ];

// NB: just one of many things that needs to be protected by
// a mutex for preemptive threads
static ostrstream tclout(outbuf, sizeof(outbuf));

#undef DO
#define DO(x)							\
{								\
    w_rc_t ___rc = x;						\
    if (___rc)  {						\
	tclout.seekp(ios::beg);                                 \
	tclout << ssh_err_name(___rc.err_num()) << ends;	\
	Tcl_AppendResult(ip, tclout.str(), 0);			\
	return TCL_ERROR;					\
    }								\
}

#undef DO_GOTO
#define DO_GOTO(x)  						\
{								\
    w_rc_t ___rc = x;						\
    if (___rc)  {							\
	tclout.seekp(ios::beg);                                 \
	tclout << ssh_err_name(___rc.err_num()) << ends;		\
	Tcl_AppendResult(ip, tclout.str(), 0);			\
	goto failure;						\
    }								\
}

#define TCL_HANDLE_FSCAN_FAILURE(f_scan) 			\
    if(f_scan==0 || f_scan->error_code()) { 			\
       w_rc_t err; 						\
       if(f_scan) {  						\
	    err = f_scan->error_code(); 			\
	    delete f_scan; f_scan =0;  				\
	} else { 						\
	    err = RC(fcOUTOFMEMORY); 				\
	} 							\
	DO(err);						\
    }

#define HANDLE_FSCAN_FAILURE(f_scan)				\
    if(f_scan==0 || f_scan->error_code()) {			\
       w_rc_t err; 						\
       if(f_scan) { 						\
	    err = f_scan->error_code(); 			\
	    delete f_scan; f_scan =0;  				\
	    return err; 					\
	} else { 						\
	    return RC(fcOUTOFMEMORY); 				\
	} 							\
    }
    
extern void   dump_pin_hdr(ostream &out, pin_i &handle); 
extern void   dump_pin_body(ostrstream &out, pin_i &handle,
	smsize_t start, smsize_t length, Tcl_Interp *ip); 
extern w_rc_t dump_scan(scan_file_i &scan, ostream &out); 
extern bool tcl_scan_boolean(char *rep, bool &result);
extern vec_t & parse_vec(const char *c, int len) ;
extern vid_t make_vid_from_lvid(const char* lv);
extern ss_m::ndx_t cvt2ndx_t(char* s);
extern lockid_t cvt2lockid_t(char* str);
extern bool use_logical_id(Tcl_Interp* ip);

inline 
void tcl_append_boolean(Tcl_Interp *ip, bool flag)
{
    Tcl_AppendResult(ip, flag ? "TRUE" : "FALSE", 0); 
}

inline int
streq(char* s1, char* s2)
{
    return !strcmp(s1, s2);
}

//ss_m::ndx_t cvt2ndx_t(char* s)

#ifdef SSH_VERBOSE
inline const char *
cvt2string(scan_index_i::cmp_t i)
{
    switch(i) {
    case scan_index_i::gt: return ">";
    case scan_index_i::ge: return ">=";
    case scan_index_i::lt: return "<";
    case scan_index_i::le: return "<=";
    case scan_index_i::eq: return "==";
    default: return "BAD";
    }
    return "BAD";
}
#endif
inline scan_index_i::cmp_t 
cvt2cmp_t(char* s)
{
    if (streq(s, ">"))  return scan_index_i::gt;
    if (streq(s, ">=")) return scan_index_i::ge;
    if (streq(s, "<"))  return scan_index_i::lt;
    if (streq(s, "<=")) return scan_index_i::le;
    if (streq(s, "==")) return scan_index_i::eq;
    return scan_index_i::bad_cmp_t;
}


inline lock_mode_t
cvt2lock_mode(char* s)
{
    for (int i = lock_base_t::MIN_MODE; i <= lock_base_t::MAX_MODE; i++)  {
	if (strcmp(s, lock_base_t::mode_str[i]) == 0)
	    return (lock_mode_t) i;
    }
    cerr << "cvt2lock_mode: bad lock mode" << endl;
    W_FATAL(ss_m::eINTERNAL);
    return IS;
}

inline lock_duration_t 
cvt2lock_duration(char* s)
{
    for (int i = 0; i < lock_base_t::NUM_DURATIONS; i++) {
	if (strcmp(s, lock_base_t::duration_str[i]) == 0)
	    return (lock_duration_t) i;
    }

    cerr << "cvt2lock_duration: bad lock duration" << endl;
    W_FATAL(ss_m::eINTERNAL);
    return t_long;
}

inline ss_m::sm_store_property_t
cvt2store_property(char* s)
{
    ss_m::sm_store_property_t prop = ss_m::t_regular;
    if (strcmp(s, "tmp") == 0)  {
	prop = ss_m::t_temporary;
    } else if (strcmp(s, "regular") == 0)  {
	prop = ss_m::t_regular;
    } else if (strcmp(s, "load_file") == 0) {
	prop = ss_m::t_load_file;
    } else if (strcmp(s, "insert_file") == 0) {
	prop = ss_m::t_insert_file;
    } else {
	cerr << "bad store property: " << s << endl;
	W_FATAL(ss_m::eINTERNAL);
    }
    return prop;
}

inline nbox_t::sob_cmp_t 
cvt2sob_cmp_t(char* s)
{
    if (streq(s, "||"))  return nbox_t::t_overlap;
    if (streq(s, "/")) return nbox_t::t_cover;
    if (streq(s, "=="))  return nbox_t::t_exact;
    if (streq(s, "<<")) return nbox_t::t_inside;
    return nbox_t::t_bad;
}

inline ss_m::concurrency_t 
cvt2concurrency_t(char* s)
{
    if (streq(s, "t_cc_none"))  return ss_m::t_cc_none;
    if (streq(s, "t_cc_record"))  return ss_m::t_cc_record;
    if (streq(s, "t_cc_page"))  return ss_m::t_cc_page;
    if (streq(s, "t_cc_file"))  return ss_m::t_cc_file;
    if (streq(s, "t_cc_kvl"))  return ss_m::t_cc_kvl;
    if (streq(s, "t_cc_modkvl"))  return ss_m::t_cc_modkvl;
    if (streq(s, "t_cc_im"))  return ss_m::t_cc_im;
    if (streq(s, "t_cc_append"))  return ss_m::t_cc_append;
    return ss_m::t_cc_bad;
}

inline int
check(Tcl_Interp* ip, const char* s, int ac, int n1, int n2 = 0, int n3 = 0,
      int n4 = 0, int n5 = 0)
{
    if (ac != n1 && ac != n2 && ac != n3 && ac != n4 && ac != n5) {
	if (s[0])  {
	    Tcl_AppendResult(ip, "wrong # args; should be \"", s,
			     "\"", 0);
	} else {
	    Tcl_AppendResult(ip, "wrong # args, none expected", 0);
	}
	return -1;
    }
    return 0;
}

enum typed_btree_test {
    test_nosuch, 
    test_i1, test_i2, test_i4, 
    test_u1, test_u2, test_u4, 
    test_f4, test_f8, 
    // selected byte lengths v=variable
    test_b1, test_b23, test_bv, 
    test_spatial
};

extern "C" {
    int t_test_bulkload_int_btree(Tcl_Interp* ip, int ac, char* av[]);
    int t_test_int_btree(Tcl_Interp* ip, int ac, char* av[]);
    int t_test_typed_btree(Tcl_Interp* ip, int ac, char* av[]);
    int t_sort_file(Tcl_Interp* ip, int ac, char* av[]);
    int t_create_typed_rec(Tcl_Interp* ip, int ac, char* av[]);
    int t_create_typed_hdr_rec(Tcl_Interp* ip, int ac, char* av[]);
    int t_scan_sorted_recs(Tcl_Interp* ip, int ac, char* av[]);
    int t_compare_typed(Tcl_Interp* ip, int ac, char* av[]);
    int t_find_assoc_typed(Tcl_Interp* ip, int ac, char* av[]);
    int t_get_store_info(Tcl_Interp* ip, int ac, char* av[]);
    typed_btree_test cvt2type(const char* s);
    void cvt2typed_value( typed_btree_test t, 
				char *string, typed_value &);
    int check_key_type(Tcl_Interp *ip, typed_btree_test t, 
	const char *given, const char *stidstring);
    typed_btree_test get_key_type(Tcl_Interp *ip,  const char *stidstring );
    char* cvt_concurrency_t( ss_m::concurrency_t cc);
    char* cvt_ndx_t( ss_m::ndx_t cc);
    char* cvt_store_t(ss_m::store_t n);

}


