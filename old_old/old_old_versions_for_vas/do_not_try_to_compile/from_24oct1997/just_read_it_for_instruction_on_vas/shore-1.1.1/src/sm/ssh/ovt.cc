/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: ovt.cc,v 1.40 1997/06/15 10:30:15 solomon Exp $
 */
#define OVT_C
#include <string.h>
#include <tcl.h>

#include <shore.def>	/* Extract USE_VERIFY */

#ifdef USE_VERIFY
#include "sm_vas.h"
#include "ovt.h"
#include <stime.h>

#include <gdbm.h>

#define	GENERIC_BUF	100

#if defined(_POSIX_PATH_MAX)
#define	PATH_BUF	_POSIX_PATH_MAX
#elif defined(MAXPATHLEN)
#define	PATH_BUF	MAXPATHLEN
#else
#define	PATH_BUF	1024
#endif


const char* pdb_tag = "permdb";
const char* tdb_tag = "tmpdb";
char pdb_name[PATH_BUF];
char tdb_name[PATH_BUF];

static GDBM_FILE permdb = 0;
static GDBM_FILE tmpdb = 0;

static void ovt_fatal(
    const char* const s,
    const char* const file,
    int line)
{
    cerr << file << '.' << line << ": " << s << endl;
    cerr << "ovt error detected ... exiting" << endl;
    W_FATAL(fcINTERNAL);
}

#define UNKNOWN		'u'
#define YES		'd'
#define NO		'-'
#define FIDLEN		20		

#define OVT_ERROR(s)  ovt_fatal(s, __FILE__, __LINE__)

#undef DO
#define DO(x) {								      \
    if (x)  {								      \
	OVT_ERROR(#x);							      \
    }									      \
}

typedef int ovtproc_t(Tcl_Interp* ip, int ac, char* av[]);

typedef struct {
    int chksum;
    int len;
    char ch;
    char fid[FIDLEN];
    char deleted;
} ovt_data_t;

    
static int
check(Tcl_Interp* ip, char* s, int ac, int n1, int n2 = 0, int n3 = 0)
{
    if (ac != n1 && ac != n2 && ac != n3) {
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

static int
ovt_begin_xct(Tcl_Interp* ip, int ac, char*[])
{
    if (check(ip, "", ac, 1))
	return TCL_ERROR;
    
    assert(tmpdb == 0);
    tmpdb = gdbm_open(tdb_name, 512, GDBM_NEWDB, 0666, NULL);

    if (! tmpdb) {
	OVT_ERROR("ovt_begin_xct: gdbm_open failed");
    }

    return TCL_OK;
}

static int
ovt_commit_xct(Tcl_Interp* ip, int ac, char*[])
{
    char tmp[100];

    if (check(ip, "", ac, 1))
	return TCL_ERROR;

    if(!permdb) return TCL_OK;

    for (datum k = gdbm_firstkey(tmpdb); k.dptr; k = gdbm_nextkey(tmpdb, k)) {

	datum d = gdbm_fetch(tmpdb, k);

	if ( ((ovt_data_t *)d.dptr)->deleted == YES ) {
	    gdbm_delete(permdb, k);	// if no entry exists in permdb, fine
	} else {
	    DO(gdbm_store(permdb, k, d, GDBM_REPLACE));
	}
	delete(d.dptr);

	w_assert1(k.dsize < (int)sizeof(tmp)); 
	memset(tmp, '\0', sizeof(tmp));
	memcpy(tmp, k.dptr, k.dsize);
	delete(k.dptr); 
	k.dptr = tmp;
    }
    
    gdbm_close(tmpdb);
    tmpdb = 0;

    return TCL_OK;
}

static int
ovt_abort_xct(Tcl_Interp* ip, int ac, char* [])
{
    if (check(ip, "", ac, 1))
	return TCL_ERROR;
   
    if (tmpdb) {
    	gdbm_close(tmpdb);
    	tmpdb = 0;
    }

    return TCL_OK;
}

static int
ovt_pop(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "key", ac, 2))
	return TCL_ERROR;

    datum k;
    k.dptr = av[1];
    k.dsize = strlen(av[1]);

    ovt_data_t data;
    data.deleted = YES;
    data.chksum = 0;
    data.len = 0;
    data.ch = '\0';
    memset(data.fid, '\0', FIDLEN);

    datum d;
    d.dptr = (char *)&data;
    d.dsize = sizeof(data);

    DO(gdbm_store(tmpdb, k, d, GDBM_REPLACE));

    return TCL_OK;
}

static int
ovt_put(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "key chksum len start_char fid", ac, 6))
	return TCL_ERROR;

    ovt_data_t data;
    data.chksum = atoi(av[2]);
    data.len = atoi(av[3]);
    data.ch = av[4][0];
    data.deleted = NO;
    memset(data.fid, '\0', FIDLEN);
    strcpy(data.fid, av[5]);

    datum d;
    d.dptr = (char*) &data, d.dsize = sizeof(data);

    datum k;
    k.dptr = (char*) av[1], k.dsize = strlen(av[1]);

    DO(gdbm_store(tmpdb, k, d, GDBM_REPLACE));

    return TCL_OK;
}

static int
ovt_peek(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "key", ac, 2))
	return TCL_ERROR;
    if(!permdb) return TCL_OK;
    
    datum k;
    k.dptr = av[1];
    k.dsize = strlen(av[1]);

    char buf[GENERIC_BUF];
    memset(buf, 0, sizeof(buf));
    ostrstream o(buf, sizeof(buf));

    datum d = gdbm_fetch(tmpdb, k);

    if (!d.dptr) {			// not in tmpdb
	d = gdbm_fetch(permdb, k);
    }

    if (!d.dptr) {
	o << UNKNOWN << '\0';
    } else {
	ovt_data_t *data = new ovt_data_t;
	memcpy(data, d.dptr, sizeof(ovt_data_t));
	delete(d.dptr);

	o << data->deleted << ' ' << av[1] << ' ' << data->chksum << ' '
		<< data->len << ' ' << data->ch << ' ' << data->fid << '\0';

	delete(data);
    }

    Tcl_AppendResult(ip, buf, 0);
    return TCL_OK;
}

void scan_ovt(Tcl_Interp* ip, GDBM_FILE ndbm, char *fid)
{
    char buf[GENERIC_BUF];
    memset(buf, 0, sizeof(buf));
    
    ostrstream o(buf, sizeof(buf));
    ovt_data_t *data = new ovt_data_t;
    char tmp[GENERIC_BUF];
    memset(tmp, 0, sizeof(tmp));

    for (datum k = gdbm_firstkey(ndbm); k.dptr; k = gdbm_nextkey(ndbm, k))
    {
	datum d = gdbm_fetch(ndbm, k);
        memcpy(data, d.dptr, sizeof(ovt_data_t));
	delete(d.dptr);

	w_assert1(k.dsize < (int)sizeof(tmp));
	memset(tmp, '\0', sizeof(tmp));
	memcpy(tmp, k.dptr, k.dsize);

	delete(k.dptr); 
	k.dptr = tmp;
	
	if (!strcmp(fid, data->fid)) {
	    o.seekp(0);

	    o << data->deleted << ' ' << tmp << ' ' << data->chksum
		<< ' ' << data->len << ' ' << data->ch << '\0';

	    Tcl_AppendElement(ip, buf);
	}
    }
    delete(data);
}

static int
ovt_tmpovt(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "fid", ac, 2))
	return TCL_ERROR;

    scan_ovt(ip, tmpdb, av[1]);

    return TCL_OK;
}

static int
ovt_permovt(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "fid", ac, 2))
	return TCL_ERROR;

    if(!permdb) return TCL_OK;

    scan_ovt(ip, permdb, av[1]);

    return TCL_OK;
}

//
//	chksum 0 BCD	returns the checksum of record "BCD" 
//	chksum 10 MN	returns the checksum of an appended or truncated
//			pattern "MN"
//
static int
ovt_chksum(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "start_pos data", ac, 3))
	return TCL_ERROR;

    int start_pos = atoi(av[1]) + 1;
    int len = strlen(av[2]);
    int chksum = 0;

    for (int i = 0; i < len; i++)  {
	chksum = chksum ^ ((i + start_pos) * av[2][i]);
    }
    
    char buf[GENERIC_BUF];
    ostrstream s1(buf, sizeof(buf));
    s1 << chksum << '\0';

    Tcl_AppendResult(ip, buf, 0);
    return TCL_OK;
}

//
//	mkdata 5	returns a pattern of size 5
//	mkdata 5 B 4	returns a pattern of size 5 to append to "BCDE"
//	mkdata -3 B 5	returns the pattern to truncate from "BCDEF"
//
static int
ovt_mkdata(Tcl_Interp* ip, int ac, char* av[])
{
    const pat_sz = 37;
    static char pattern[pat_sz + 1] = { 0 };

    if (pattern[0] == '\0')  {
	int i;
	for (i = 0; i < pat_sz; i++) 
	    pattern[i] = '6' + i;
	pattern[i] = '\0';
    }

    if (check(ip, "len [1st_char orig_len]", ac, 2, 4))  {
	return TCL_ERROR;
    }

    //
    //  all that are needed to generate data is len (length), and start (the
    //  offset of the beginning data character in the pattern)
    //
    int len = atoi(av[1]);
    int start; 

    if (ac == 2)  {	// generate a rec 
	assert(len > 0);
	start = rand() % pat_sz;
    }
    else  {		// ac is 4, to return pattern to truncate or append
	int olen = atoi(av[3]); 	// current length of rec
	assert(olen >= 0);

	// index in pattern of 1st char in rec
	int oindex = av[2][0] - pattern[0];

	if (len < 0) {		// to truncate rec
	    len = -len;
	    assert(olen >= len); 

	    // index of 1st char of truncated string 
	    start = (oindex + olen - len) % pat_sz;
	}
	else {			// to append rec

	    // index of 1st char of appended string 
	    start = (oindex + olen) % pat_sz;
	}
    }

    Tcl_ResetResult(ip);

    char *result = new char [ len + 1 ];
    memset(result, '\0', len + 1);

    for (int i = 0; i < len; ++i)  {
	result[i] = pattern[ (start + i) % pat_sz ];
    }
    Tcl_AppendResult(ip, result, 0);
    delete(result);
    assert(abs(atoi(av[1])) == strlen(ip->result));
    return TCL_OK;
}

static int
ovt_newovt(Tcl_Interp* ip, int ac, char* /*av*/[])
{
    if (check(ip, "", ac, 1))
	return TCL_ERROR;

    if (permdb) {
	gdbm_close(permdb);
    }

    permdb = gdbm_open(pdb_name, 512, GDBM_NEWDB, 0666, NULL);
    
    if (! permdb) {
	cerr << "ovt_newovt: gdbm_open failed" << endl;
	W_FATAL(fcOS);
    }
    tmpdb = 0;

    return TCL_OK;
}

struct cmd_t {
    char*	name;
    ovtproc_t*	func;
};

static cmd_t ovt_cmd[] = {
    {"mkdata", ovt_mkdata},
    {"chksum", ovt_chksum},
    {"newovt", ovt_newovt},
    {"pop", ovt_pop},
    {"put", ovt_put},
    {"peek", ovt_peek},
    {"permovt", ovt_permovt},
    {"tmpovt", ovt_tmpovt},
    {"begin_xct", ovt_begin_xct },
    {"commit_xct", ovt_commit_xct},
    {"abort_xct", ovt_abort_xct},
    {0, 0}
};

int
ovt_dispatch(ClientData, Tcl_Interp* ip, int ac, char* av[])
{
    for (cmd_t* p = ovt_cmd; p->name; p++)  {
	if (strcmp(p->name, av[1]) == 0)  {
	    return p->func(ip, ac-1, av+1);
	}
    }
    Tcl_AppendResult(ip, "unknown routine \"", av[1], "\"", 0);
    return TCL_ERROR;
}

void ovt_init(Tcl_Interp* /*ip*/)
{
    srand(stime_t::now().secs() % 4000);

    if (permdb) {
	    cerr << "ovt_init: db already open." << endl;
	    return;
    }

    char *ovtdir = getenv("STOVT_DIR");
    if (ovtdir) {
	strcpy(pdb_name, ovtdir);
	strcat(pdb_name, "/");
	strcat(pdb_name, pdb_tag);
	strcpy(tdb_name, ovtdir);
	strcat(tdb_name, "/");
	strcat(tdb_name, tdb_tag);
    } else {
	strcpy(pdb_name, pdb_tag);
	strcpy(tdb_name, tdb_tag);
    }

    permdb = gdbm_open(pdb_name, 512, GDBM_WRCREAT, 0666, NULL);

    if (! permdb) {
	if(ovtdir) {
	    cerr << "ovt_init: gdbm_open failed" << endl;
	    W_FATAL(fcOS);
	}
	// else verify not turned on, so ignore
    }
    tmpdb = 0;
}

void ovt_final()
{
    if(permdb) {
	gdbm_close(permdb);
    }
}

#else /* !USE_VERIFY */

int
ovt_dispatch(ClientData, Tcl_Interp* ip, int /*ac*/, char* av[])
{
	Tcl_AppendResult(ip, "USE_VERIFY not configured; ovt commands unavailable\"", av[1], "\"", 0);
	return TCL_ERROR;
}

#endif
