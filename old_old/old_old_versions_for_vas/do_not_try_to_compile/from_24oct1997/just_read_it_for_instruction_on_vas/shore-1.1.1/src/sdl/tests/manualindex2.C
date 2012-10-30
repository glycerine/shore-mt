/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
// PR 251
#define MODULE_CODE
#include <iostream.h>
#include <fstream.h>
#include <std.h>
#include "ShoreApp.h"
#include "manualindex2.h"

/// NTESTS MUST BE >= 2
#define NTESTS 3 
#define NINDEXES 3 

void
usage(ostream &/*os*/, char */*progname*/)
{
    exit(1);
}

void doit(WRef<my_obj> o, enum IndexKind k);

int
main(int argc, char *argv[])
{
    char 	*progname = argv[0];
    char 	*fnameu = "my_obj_u";
    char 	*fname = "my_obj";
    shrc 	rc;

    if(argc >1 ) {
	usage(cerr, progname);
    }

    SH_DO(Shore::init(argc, argv));

    SH_BEGIN_TRANSACTION(rc);

    // If that failed, or if a transaction aborted...
    if(rc){
	// after longjmp
	cerr << rc << endl;
	return 1;
    } else {
	// The main body of the transaction goes here.

	SH_DO(Shore::chdir("/"));

	/////////////////////////////////////////////////
	// create an instance of my_obj with Btrees
	/////////////////////////////////////////////////
	cerr << "****************** TEST WITH BTREES" << endl;
	{
	    WRef<my_obj> w;
	    SH_DO( REF(my_obj)::new_persistent (fnameu, 0644, w) ) ;
	    if(!w) {
		cerr << "Cannot create " << fnameu << endl;
	    } else {
		doit(w, BTree);
	    }
	}

	/////////////////////////////////////////////////
	// create an instance of my_obj with Unique Btrees
	/////////////////////////////////////////////////
	cerr << "****************** REPEAT WITH UNIQUE BTREES" << endl;
	{
	    WRef<my_obj> w;
	    SH_DO( REF(my_obj)::new_persistent (fname, 0644, w) ) ;
	    if(!w) {
		cerr << "Cannot create " << fname << endl;
	    } else {
		doit(w, UniqueBTree);
	    }
	}
	SH_DO(SH_COMMIT_TRANSACTION);
    }

    SH_BEGIN_TRANSACTION(rc);
    if(rc){
	// after longjmp
	cerr << rc << endl;
	return 1;
    } else {
	SH_DO(Shore::unlink(fnameu));
	SH_DO(SH_COMMIT_TRANSACTION);
    }

    SH_BEGIN_TRANSACTION(rc);
    if(rc){
	// after longjmp
	cerr << rc << endl;
	return 1;
    } else {
	SH_DO(Shore::unlink(fname));
	SH_DO(SH_COMMIT_TRANSACTION);
    }
    return 0;
}

#define PERROR(rc) { if(rc && rc.err_num()!=SH_NotFound) {\
	cerr << __LINE__ << " " <<__FILE__<<":"<<endl;\
	cerr << rc << endl; if(rc = RCOK);\
	} }

void insert(WRef<my_obj> w,
	char c,
	long l,
	sdl_string &s)
{
    shrc 	rc;
    struct_t	t;
	memset(&t, 0, sizeof(t));
    t.i = l + s.strlen();
    t.c = c;

    cerr << "indx0.insert " << c << "->" << l <<endl;
    rc = w->indx0.insert(c, l);
    PERROR(rc);
    cerr << "indx1.insert " << l << "->" << l+1 << endl;
    rc = w->indx1.insert(l, l+1);
    PERROR(rc);
    cerr << "indx3.insert " << (char *)s << "->" << t.i<<","<<t.c << endl;
    rc = w->indx3.insert(s, t);
    PERROR(rc);
    cerr << "indx4.insert " << t.i <<","<<t.c <<"->" << (char *)s  << endl;
    rc = w->indx4.insert(t, s);
    PERROR(rc);
    for(long i=0; i<3; i++) {
	cerr << "a["<<i<<"].insert " << t.i+i <<"->" << i << endl;
	rc = w->a[i].insert(t.i+i, i);
	PERROR(rc);
    }
}

#define  EQ(k,_l,l,larray, idxname)\
    if(found) { eq(k,_l,l,larray,#idxname); } else { \
    cerr << #idxname << " value not found for key " << k << endl; }

void 
eq(int /*k*/, long _l, long l, long *larray, const char *idxname)
{
    bool	located=false; int m;
    if(_l != l) {
	for(m=0; m<NTESTS; m++) {
	    if(_l == larray[m]) {
		    located=true;
		    break;
	    }
	}
	if(!located) {
	    cerr << idxname << " values don't match: got "  
		<< _l << "; expected one of:"; 
	    for(m=0; m<NTESTS; m++) {
		cerr << larray[m] <<  ",";
	    }
	    cerr << endl;
	}
    } 
}

void find(WRef<my_obj> w,
	char c,
	long *larray,
	long l,
	sdl_string &s)
{
    shrc 	rc;
    struct_t	t;
	memset(&t, 0, sizeof(t));
    bool	found;
    t.i = l + s.strlen();
    t.c = c;

    struct_t	_t;
	memset(&_t, 0, sizeof(_t));
    sdl_string	_s;
    long	_i;
    long	_l;



    rc = w->indx0.find(c, _l, found);
    PERROR(rc);
    EQ(c,_l,l,larray,indx0);

    rc = w->indx1.find(l, _l, found);
    PERROR(rc);
    EQ(l,_l,l+1,larray,indx1);

    rc = w->indx3.find(s, _t, found);
    PERROR(rc);
    if(found) { 
	bool match=false;
	if(t.c == _t.c) {
	    if(t.i == _t.i) {
		match=true;
	    } else {
		// check the whole list
		for(int k=0; k<NTESTS; k++) {
		    if (larray[k] + (long)s.strlen() == _t.i) {
			match=true;
			break;
		    }
		}
	    } 
	} 
	if(!match) {
	    cerr << "indx3 values don't match; expected " 
		<< t.i <<","<<t.c <<" got "
		<< _t.i <<","<<_t.c <<" "
	    << endl; 
	}
    } else { 
	cerr << "indx3 value not found for key " << (char *)s << endl; 
    }

    //////////////////////////////////////////////////////////// 
    // expect problems with this because the data structure
    // has gaps
    //////////////////////////////////////////////////////////// 
    rc = w->indx4.find(t, _s, found);
    PERROR(rc);
    if(found)  {
	if(_s.strcmp(s)!=0) { 
	    cerr << "idx4 values don't match; got " 
		<< (char *)_s << " expected " << (char *)s<< endl; 
	} 
    } else { 
	cerr << "indx4 value not found for key " 
		<< t.i <<","<<t.c<<" " << endl; 
    }

    long junk[NTESTS]; int i;
    for(i=0; i<NTESTS; i++) {
	junk[i] = i;
    }
    for(i=0; i<NINDEXES; i++) {
	rc = w->a[i].find(t.i+i, _i, found);
	PERROR(rc);
	EQ(t.i+i,_i,i,junk,a[i]);
    }

}

void scan(WRef<my_obj> w, const char *str)
{
    shrc 	rc;
#define PKV \
	cerr << "key=" << iter.cur_key << " val=" << iter.cur_val << endl;

	// indx0 <char,int>
	cerr << "***Scan indx0 " <<str << endl;
	{
		IndexScanIter<char,long>	iter(w->indx0); // -inf to +inf
		while( !(rc = iter.next()) && !iter.eof ) { PKV; }
		PERROR(rc);
	}
	// indx1 <long,long>
	cerr << "***Scan indx1 " <<str << endl;
	{
		IndexScanIter<long,long>	iter(w->indx1); // -inf to +inf
		while( !(rc = iter.next()) && !iter.eof ) { PKV; }
		PERROR(rc);
	}
	// indx3 <string, struct_t>
	cerr << "***Scan indx3 "<<str << endl;
	{
		IndexScanIter<sdl_string,struct_t>	iter(w->indx3); // -inf to +inf
		while( !(rc = iter.next()) && !iter.eof ) { 
			cerr << "key=" << iter.cur_key << " val=" << 
				iter.cur_val.i << "," <<  iter.cur_val.c << endl;
		}
		PERROR(rc);
	}

	// indx4 <struct_t, string>
	cerr << "***Scan indx4 "<<str << endl;
	{
		IndexScanIter<struct_t,sdl_string>	iter(w->indx4); // -inf to +inf
		while( !(rc = iter.next()) && !iter.eof ) { 
			cerr << "key=" << 
				iter.cur_key.i  << "," << iter.cur_key.c
				<< " val=" << 
				iter.cur_val << endl;
		}
		PERROR(rc);
	}

	// a[i] <long, long>

    for(int i=0; i<NINDEXES; i++) {
		cerr << "***Scan indx a[" << i << "] " <<str << endl;
		index_iter<typeof(w->a[i])> iter(w->a[i]); // -inf to +inf
		while( !(rc = iter.next()) && !iter.eof ) {  PKV; }
		PERROR(rc);
    }

	cerr << "***end scan " <<str << endl;
}

void init(Ref<my_obj> o, enum IndexKind k)
{
    shrc 	rc;
    WRef<my_obj> w = o;

    //////////////////////////
    // initialize
    //////////////////////////
    rc= w->indx0.init(k);
    PERROR(rc);

    rc= w->indx1.init(k);
    PERROR(rc);

    rc= w->indx3.init(k);
    PERROR(rc);

    rc= w->indx4.init(k);
    PERROR(rc);

    for(int i=0; i<3; i++) {
	rc= w->a[i].init(k);
	PERROR(rc);
    }
}

void remove(WRef<my_obj> w,
	char c,
	long l,
	sdl_string &s)
{
    shrc 	rc;
    struct_t	t;
	memset(&t, 0, sizeof(t));
    int		num;
    t.i = l + s.strlen();
    t.c = c;

#define CHK cerr << "removed " << num << endl;

    cerr << "indx0.remove " << c << endl;
    rc = w->indx0.remove(c, num);
    PERROR(rc);
    CHK;

    cerr << "indx1.remove " << l << endl;
    rc = w->indx1.remove(l, num);
    PERROR(rc);
    CHK;

    cerr << "indx3.remove " << (char *)s << endl;
    rc = w->indx3.remove(s, num);
    PERROR(rc);
    CHK;

    cerr << "indx4.remove " << t.i <<","<<t.c  << endl;
    rc = w->indx4.remove(t, num);
    PERROR(rc);
    CHK;

    for(int i=0; i<3; i++) {
	cerr << "a["<<i<<"].remove " << t.i+i << endl;
	rc = w->a[i].remove(t.i+i, num);
	PERROR(rc);
	CHK;
    }
}

void doit(WRef<my_obj> o, enum IndexKind k)
{
    long 	_l[NTESTS]; 
    sdl_string	s = __FILE__;
    init(o,k);
    int j;

    scan(o, "START");

    for(j=0; j<NTESTS; j++) {
	_l[j]=__LINE__ + j;
    }

    for(j=2; j<NTESTS; j++) {
	insert(o,'c',_l[j],s);
    }
    // do something out of order:
    insert(o,'c',_l[0],s);
    insert(o,'c',_l[1],s);

    scan(o, "AFTER INSERTS");

    for(j=2; j<NTESTS; j++) {

	cerr << "BEGIN find/remove pass " << j << endl;

	find(o,'c',_l, _l[j],s);
	remove(o,'c',_l[j],s);
	scan(o, "AFTER find/remove pass");
    }

    // do something out of order again
    cerr << "find/remove out of order " << j << endl;
    cerr << "find c" << endl;
    find(o,'c',_l, _l[0],s);
    cerr << "remove c " << endl;
    remove(o,'c',_l[0],s);

    cerr << "find c" << endl;
    find(o,'c',_l, _l[1],s);

    cerr << "remove c" << endl;
    remove(o,'c',_l[1],s);

    scan(o, "AFTER find/removes out of order");
}

