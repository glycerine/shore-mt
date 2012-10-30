/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#define MODULE_CODE

#include <iostream.h>
#include <fstream.h>
#include <std.h>
#include "ShoreApp.h"
#include "sequences.h"

Ref<a> a_ref;
Ref<my_obj> o_ref;


# define dassert(ex)\
    {if (!(ex)){(void)fprintf(stderr,"Assertion failed: file \"%s\", line %d\n", __FILE__, __LINE__);exit(1);}}


#define CHECKLSIZE(sq,expected) \
{ size_t s=999; s = sq.get_size();\
    if(expected != s) cerr << "Expected " << expected << " got " << s << endl; \
    dassert(s == expected);\
}
#define CHECKSIZE(o_ref,sq,expected) CHECKLSIZE(o_ref.update()->sq,expected)

void
dotest () 
{
	long tmp;

    // materialize a sequence on the stack:
    seq1_t	s1;
    Sequence<long>	s2(o_ref->vseq);
	Sequence<long>	s3 = s2;

    // SEQ OPS

	CHECKLSIZE(s1,0);
	CHECKLSIZE(s2,0);
	CHECKLSIZE(s3,0);
	CHECKSIZE(o_ref,a_seq,0);
	CHECKSIZE(o_ref,vseq,0);

	s1.append_elt(); // uninit
	s1.append_elt(33);
	CHECKLSIZE(s1,2);
	// remove the uninitialized one and renumber
	s1.delete_elt(0);
	tmp = s1.get_elt(0);
	dassert(tmp==33);
	CHECKLSIZE(s1,1);

	// assignment operator
	s3 = s1;
	CHECKLSIZE(s3,1);
	s3.append_elt(44);
	s3.append_elt();
	CHECKLSIZE(s3,3);


	// repeat test with persistent object
	o_ref.update()->vseq.append_elt(); // uninit
	o_ref.update()->vseq.append_elt(99); 
	CHECKSIZE(o_ref,vseq,2);
	// remove the uninitialized one and renumber
	o_ref.update()->vseq.delete_elt(0);
	tmp = o_ref->vseq.get_elt(0);
	dassert(tmp==99);
	CHECKSIZE(o_ref,vseq,1);
	o_ref.update()->vseq.append_elt(); // uninit
	o_ref.update()->vseq.append_elt(); // uninit
	o_ref.update()->vseq.append_elt(); // uninit
	o_ref.update()->vseq.append_elt(99); 
	o_ref.update()->vseq.append_elt(99); 
	o_ref.update()->vseq.append_elt(99); 
	CHECKSIZE(o_ref,vseq,7);
	{
		int i;
		for(i=0; i<4; i++) {
			// write_elt and operator[]
			o_ref.update()->vseq.write_elt(i) = o_ref->vseq[5];
		}
		for(i=0; i<7; i++) {
			// check
			dassert(o_ref->vseq[i] == 99);
		}
		for(i=0; i<7; i++) {
			// write_elt 
			o_ref.update()->vseq.write_elt(i) = i;
		}
		for(i=0; i<7; i++) {
			// operator[]
			dassert(o_ref->vseq[i] == i);
		}
	}

	// assignment operator
	s3 = o_ref->vseq;
	CHECKLSIZE(s3,7);
	{
		size_t sx = s1.get_size();
		// how many ways can we check this???
		dassert(sx==1);
		CHECKLSIZE(s1,1);

		o_ref.update()->vseq = s1;
		CHECKLSIZE(s1,1);
		CHECKLSIZE(o_ref->vseq,sx);
		dassert(sx==1);
		CHECKLSIZE(o_ref->vseq,1);
	}

#ifndef NOTDEF
	// aseq : TODO: what happens to uninit ref?
	// how do you check it?

    o_ref.update()->a_seq.append_elt();  // uninitialized
    o_ref.update()->a_seq.append_elt();  // uninitialized
    o_ref.update()->a_seq.append_elt(a_ref);
	CHECKSIZE(o_ref,a_seq,3);
    { 
		if(o_ref->a_seq.get_size() == 2) {
			// these 2 are identical :
			a_ref = o_ref->a_seq.get_elt(0);
			a_ref = o_ref->a_seq[0];
			o_ref.update()->a_seq.delete_elt(0); // renumbers member [1] to [0]
			o_ref.update()->a_seq.delete_elt(0); // removes what was member [1]
		}
    }

    { long i;
	// correct but inefficient because of renumbering:
	for (i=0; (unsigned long)i < o_ref->a_seq.get_size(); i++) 
		o_ref.update()->a_seq.delete_elt(0);

	// correct and doesn't renumber elements each time:
	for (i=o_ref->a_seq.get_size()-1; i>=0; i--) 
		o_ref.update()->a_seq.delete_elt(i);
    }
#endif
}

void
usage(ostream &/*os*/, char */*progname*/)
{
    exit(1);
}
void
destroy_obj(const char *fname) 
{
	shrc rc;
    // 
    // Another transaction to destroy the file...
    //
    SH_BEGIN_TRANSACTION(rc);
    if(rc){
	// after longjmp
	cerr << rc << endl;
	return;
    } else {
	SH_DO(Shore::unlink(fname));
	SH_DO(SH_COMMIT_TRANSACTION);
    }
}

int
main(int argc, char *argv[])
{
    char 	*progname = argv[0];
    char 	*fname;
    shrc 	rc;

    if(argc != 1){
	usage(cerr, progname);
    }

    // Establish a connection with the vas and initialize 
    // the object  cache.
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

	//////////////////////////////////
	// GUTS HERE
	//////////////////////////////////
	fname = "XXX";
	{
	    bool ok = true;

	    SH_DO( REF(my_obj)::new_persistent (fname, 0644, o_ref) ) ;
	    if(!o_ref ) {
		cerr << "Cannot create new objects " << fname << endl;
		ok = false;
	    } else {
		SH_DO( REF(a)::new_persistent ("a_ref_junk", 0644, a_ref) ) ;
		if(!a_ref ) {
		    cerr << "Cannot create new objects " << "a_ref_junk" << endl;
		    ok = false;
		} 
	    }
	    dotest();

	}
	SH_DO(SH_COMMIT_TRANSACTION);
    }

    destroy_obj(fname);
    destroy_obj("a_ref_junk");
    return 0;
}
