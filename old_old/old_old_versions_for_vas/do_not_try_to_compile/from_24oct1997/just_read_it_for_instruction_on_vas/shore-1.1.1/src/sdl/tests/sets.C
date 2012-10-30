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
#include "sets.h"

bool debug=false;

// ok up to about 10
#define NOBJS 3
Ref<a> a_ref, tmp_a_ref, a_ref_array[NOBJS];
size_t final_set_size, final_bag_size;

const char *names = "01234567890aref_name";

LOID	a_loid_array[NOBJS+2]; 
char *string_representation[NOBJS+2]; 

Ref<my_obj> o_ref;

Set<Ref<a> > *local_setp;
Bag<Ref<a> > *local_bagp;

# define dassert(ex)\
    {if (!(ex)){(void)fprintf(stderr,"Assertion failed: file \"%s\", line %d\n", __FILE__, __LINE__);exit(1);}}


#define CHECKLSIZE(a_set,expected) \
{ size_t s=999; s = a_set.get_size();\
    if((size_t)expected != s) cerr << "Expected " << expected << " got " << s << endl; \
    dassert(s == (size_t)expected);\
}
#define CHECKSIZE(o_ref,a_set,expected) CHECKLSIZE(o_ref.update()->a_set,expected)

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
#ifdef notdef
	    set_tests(Set<a> &a_set, const Set<a> &ca_set)) 
	    {
		CHECKSIZE(o_ref,a_set,0);

		// This is a set-- add something twice and it
		// should appear only once
		//
		o_ref.update()->a_set.add(a_ref);
		o_ref.update()->a_set.add(a_ref);
		CHECKSIZE(o_ref,a_set,1);

		// Likewise, we should be able to something twice
		// with no effect:
		// add another just for yuks
		o_ref.update()->a_set.add(a_ref_array[0]);
		CHECKSIZE(o_ref,a_set,2);
		o_ref.update()->a_set.del(a_ref);
		CHECKSIZE(o_ref,a_set,1);
		o_ref.update()->a_set.del(a_ref);
		CHECKSIZE(o_ref,a_set,1);

		tmp_a_ref = o_ref.update()->a_set.delete_one();
		CHECKSIZE(o_ref,a_set,0);

		o_ref.update()->a_set.add(a_ref);
		o_ref.update()->a_set.add(a_ref_array[0]);
		CHECKSIZE(o_ref,a_set,2);
		tmp_a_ref = o_ref.update()->a_set.delete_one();
		CHECKSIZE(o_ref,a_set,1);
		tmp_a_ref = o_ref.update()->a_set.delete_one();
		CHECKSIZE(o_ref,a_set,0);

		for(i=0; i<NOBJS; i++ ) {
		    o_ref.update()->a_set.add(a_ref_array[i]);
		}
		CHECKSIZE(o_ref,a_set,NOBJS);

		for(i=NOBJS-1; i>=0; i-- ) {
		    tmp_a_ref = o_ref->a_set.get_elt(i);
		    dassert(tmp_a_ref == a_ref_array[i]);
		    dassert(tmp_a_ref != a_ref); 
		    // PR 261: member() should be const -- dassert(o_ref->a_set.member(tmp_a_ref));
		    dassert(o_ref.update()->a_set.member(tmp_a_ref));
		}
		CHECKSIZE(o_ref,a_set,NOBJS);

		// a_ref is not a member
		// PR 261: member() should be const -- dassert(!  o_ref->a_set.member(a_ref) );
		dassert( ! o_ref.update()->a_set.member(a_ref) );

		for(i=NOBJS-1; i>=0; i-- ) {
		    CHECKSIZE(o_ref,a_set,i+1);
		    // del(elt) looks for element elt
		    o_ref.update()->a_set.del(a_ref_array[i]);
		    CHECKSIZE(o_ref,a_set,i);
		}
		CHECKSIZE(o_ref,a_set,0);

		/// Sigh, ok, fill in a few just for checking in the next tx
		for(i=0; i<NOBJS; i++ ) {
		    o_ref.update()->a_set.add(a_ref_array[i]);
		}
		CHECKSIZE(o_ref,a_set,NOBJS);
		final_set_size = NOBJS;
	    }
#endif
void
set_tests(Set<Ref<a> > &a_set) {
    int i;
    CHECKLSIZE(a_set,0);

    // This is a set-- add something twice and it
    // should appear only once
    //
    a_set.add(a_ref);
    a_set.add(a_ref);
    CHECKLSIZE(a_set,1);

    // Likewise, we should be able to something twice
    // with no effect:
    // add another just for yuks
    a_set.add(a_ref_array[0]);
    CHECKLSIZE(a_set,2);
    a_set.del(a_ref);
    CHECKLSIZE(a_set,1);
    a_set.del(a_ref);
    CHECKLSIZE(a_set,1);

    tmp_a_ref = a_set.delete_one();
    CHECKLSIZE(a_set,0);

    a_set.add(a_ref);
    a_set.add(a_ref_array[0]);
    CHECKLSIZE(a_set,2);
    tmp_a_ref = a_set.delete_one();
    CHECKLSIZE(a_set,1);
    tmp_a_ref = a_set.delete_one();
    CHECKLSIZE(a_set,0);

    for(i=0; i<NOBJS; i++ ) {
	a_set.add(a_ref_array[i]);
    }
    CHECKLSIZE(a_set,NOBJS);

    for(i=NOBJS-1; i>=0; i-- ) {
	tmp_a_ref = o_ref->a_set.get_elt(i);
	dassert(tmp_a_ref == a_ref_array[i]);
	dassert(tmp_a_ref != a_ref); 
	dassert(a_set.member(tmp_a_ref));
    }
    CHECKLSIZE(a_set,NOBJS);

    dassert( ! a_set.member(a_ref) );

    for(i=NOBJS-1; i>=0; i-- ) {
	CHECKLSIZE(a_set,i+1);
	// del(elt) looks for element elt
	a_set.del(a_ref_array[i]);
	CHECKLSIZE(a_set,i);
    }
    CHECKLSIZE(a_set,0);

    /// Sigh, ok, fill in a few just for checking in the next tx
    for(i=0; i<NOBJS; i++ ) {
	a_set.add(a_ref_array[i]);
    }
    CHECKLSIZE(a_set,NOBJS);
    final_set_size = NOBJS;
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

    local_setp = new Set<Ref<a> >;
    local_bagp = new Bag<Ref<a> >;

    Set<Ref<a> > & local_set = *local_setp;
    Bag<Ref<a> > & local_bag = *local_bagp;

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
	    int i;

	    SH_DO( REF(my_obj)::new_persistent (fname, 0644, o_ref) ) ;
	    if(!o_ref ) {
		cerr << "Cannot create new objects " << fname << endl;
		ok = false;
	    } else {
		SH_DO( REF(a)::new_persistent ("a_ref_junk", 0644, a_ref) ) ;
		if(!a_ref ) {
		    cerr << "Cannot create new objects " << "a_ref_junk" << endl;
		    ok = false;
		} else for(i=0; ok && (i<NOBJS); i++) {
		    a_ref.update()->longattr = NOBJS;
		    SH_DO( REF(a)::new_persistent (&names[i], 0644, a_ref_array[i]) ) ;
		    if(a_ref_array[i]) {
			a_ref_array[i].update()->longattr = i;
		    } else {
			cerr << "Cannot create new a_ref object " << &names[i] << endl;
			ok = false;
		    }
		}
	    }
	    ///////// STASH THE STRING REPRESENTATIONS OF THE OIDs
	    {
		char buf[100]; // better big enough !
		ostrstream out(buf,100);

		// o_ref
		LOID	loid;
		i = NOBJS+1;

		SH_DO( o_ref.get_loid (loid) ) ;
		out.seekp(ios::beg);
		out << loid << ends;
		if(debug) cerr << "o_ref" << " is " << loid << endl;
		string_representation[i] = new char[strlen(buf)+1];
		memcpy(string_representation[i], buf, strlen(buf)+1);
		if(debug) cerr << "string rep" << " is " << loid << endl;
		a_loid_array[i] = loid;

		// a_ref
		i = NOBJS;
		SH_DO( a_ref.get_loid (loid) ) ;
		out.seekp(ios::beg);
		out << loid << ends;
		if(debug) cerr << "a_ref" << " is " << loid << endl;
		string_representation[i] = new char[strlen(buf)+1];
		memcpy(string_representation[i], buf, strlen(buf)+1);
		if(debug) cerr << "string rep" << " is " << loid << endl;
		a_loid_array[i] = loid;

		// a_ref[i]
		for(i=0; i<NOBJS; i++ ) {
		    SH_DO( a_ref_array[i].get_loid (loid) ) ;
		    out.seekp(ios::beg);
		    out << loid << ends;
		    if(debug) cerr << "a_ref_array["<<i<<"]" << " is " << loid << endl;
		    string_representation[i] = new char[strlen(buf)+1];
		    memcpy(string_representation[i], buf, strlen(buf)+1);
		    if(debug) cerr << "string rep" << " is " << loid << endl;
		    a_loid_array[i] = loid;
		}
	    }
	    //////////////////////////////////////////////////////

	    // INITIALIZE AND USE objects here
	    if(ok) {
		///////// SET OPS ///////////////////
		o_ref.update()->a_set.add(a_ref);
		// PR 261: member() should be const -- 
		dassert(o_ref.update()->a_set.member(a_ref));

		o_ref.update()->a_set = local_set;
		cout << endl << "set_tests(o_ref.update()->a_set);" << endl;

		set_tests(o_ref.update()->a_set);
		cout << endl << " set_tests(local_set);" <<endl;
		set_tests(local_set);

		///////////////// END OF SET //////////////////

		///////////////// BAG //////////////////
		CHECKSIZE(o_ref,a_bag,0);
		CHECKLSIZE(local_bag,0);

		// It's a bag -- elt should appear twice
		// if it's inserted twice
		o_ref.update()->a_bag.add(a_ref);
		o_ref.update()->a_bag.add(a_ref);
		o_ref.update()->a_bag.add(a_ref);
		CHECKSIZE(o_ref,a_bag,3);
		// del(elt) looks for element elt
		o_ref.update()->a_bag.del(a_ref);
		CHECKSIZE(o_ref,a_bag,2);

		tmp_a_ref = o_ref.update()->a_bag.delete_one();
		CHECKSIZE(o_ref,a_bag,1);

		o_ref.update()->a_bag.add(a_ref);
		o_ref.update()->a_bag.add(a_ref);
		CHECKSIZE(o_ref,a_bag,3);
		tmp_a_ref = o_ref.update()->a_bag.delete_one();
		CHECKSIZE(o_ref,a_bag,2);
		tmp_a_ref = o_ref.update()->a_bag.delete_one();
		CHECKSIZE(o_ref,a_bag,1);
		tmp_a_ref = o_ref.update()->a_bag.delete_one();
		CHECKSIZE(o_ref,a_bag,0);

		for(i=0; i<NOBJS; i++ ) {
		    o_ref.update()->a_bag.add(a_ref_array[i]);
		}
		CHECKSIZE(o_ref,a_bag,NOBJS);
		o_ref.update()->a_bag.add(a_ref);
		CHECKSIZE(o_ref,a_bag,NOBJS+1);

		{ // demo idiom for set or bag:
		    for (tmp_a_ref = o_ref->a_bag.get_elt(i=0);
			    tmp_a_ref != NULL;
			    tmp_a_ref = o_ref->a_bag.get_elt(++i))
		    { 
			// PR 261: member() should be const -- 
			// dassert(o_ref->a_bag.member(tmp_a_ref)!=0);
			dassert(o_ref.update()->a_bag.member(tmp_a_ref)!=0);
		    }
		}

		for(i=NOBJS-1; i>=0; i-- ) {
		    tmp_a_ref = o_ref->a_bag.get_elt(i);
		    dassert(tmp_a_ref == a_ref_array[i]);
		    dassert(tmp_a_ref != a_ref); 
		    // PR 261: member() should be const -- 
		    // dassert(o_ref->a_bag.member(tmp_a_ref));
		    dassert(o_ref.update()->a_bag.member(tmp_a_ref));
		}
		CHECKSIZE(o_ref,a_bag,NOBJS+1);

		// PR 261: member() should be const -- 
		// dassert( o_ref->a_bag.member(a_ref) );
		dassert( o_ref.update()->a_bag.member(a_ref) );

		// del(elt) looks for element elt
		o_ref.update()->a_bag.del(a_ref);
		dassert( !o_ref.update()->a_bag.member(a_ref) );

		for(i=NOBJS-1; i>=0; i-- ) {
		    // del(elt) looks for element elt
		    o_ref.update()->a_bag.del(a_ref_array[i]);
		    CHECKSIZE(o_ref,a_bag,i);
		}
		CHECKSIZE(o_ref,a_bag,0);

		/// Sigh, ok, fill in just for checking in the next tx
		for(i=0; i<NOBJS; i++ ) {
		    o_ref.update()->a_bag.add(a_ref_array[i]);
		}
		CHECKSIZE(o_ref,a_bag,NOBJS);
		final_bag_size = NOBJS;
	    }
	}
	SH_DO(SH_COMMIT_TRANSACTION);
    }

    ///////////// CHECK AGAIN ////////////////

    SH_BEGIN_TRANSACTION(rc);

    // If that failed, or if a transaction aborted...
    if(rc){
	// after longjmp
	cerr << rc << endl;
	return 1;
    } else {
	// The main body of the transaction goes here.

	SH_DO(Shore::chdir("/"));

	{
	    bool ok = true;
	    int i;
	    LOID	loid;

	    //////// Try to locate the objects by LOID
	    {
		Ref<any> anyref;

		// o_ref
		{
		    Ref<my_obj> tmp;
		    istrstream anon(string_representation[NOBJS+1], 
			strlen(string_representation[NOBJS+1]));
		    anon >> loid;

		    anyref = loid;
		    tmp = TYPE_OBJECT(my_obj).isa(anyref);
		    SH_DO( tmp.fetch() ) ;

		    /////////// Now look up an alternative way
		    SH_DO( REF(my_obj)::lookup (fname, o_ref) ) ;

		    dassert(o_ref == tmp);
		    dassert(o_ref.valid() == RCOK);
		}

		// a_ref
		{
		    istrstream anon(string_representation[NOBJS], 
			strlen(string_representation[NOBJS]));
		    anon >> loid;
		    anyref = loid;
		    a_ref = TYPE_OBJECT(a).isa(anyref);
		    SH_DO(a_ref.fetch());
		    dassert(a_ref->longattr == NOBJS);
		}

		// a_ref[i]
		for(i=0; i<NOBJS; i++ ) {
		    {
			istrstream anon(string_representation[i], 
			    strlen(string_representation[i]));
			anon>> loid;
		    }
		    anyref = loid;
		    a_ref_array[i] = TYPE_OBJECT(a).isa(anyref);
		    dassert(a_ref_array[i]->longattr == i);
		    SH_DO(a_ref_array[i].fetch());
		}
	    }
	    if(ok) {
		LOID	loid;
		///////// CHECK ///////////////////
		CHECKSIZE(o_ref,a_set,final_set_size);
		CHECKSIZE(o_ref,a_bag,final_bag_size);

		for(i=NOBJS-1; i>=0; i-- ) {
		    tmp_a_ref = o_ref->a_set.get_elt(i);
		    SH_DO(tmp_a_ref.get_loid(loid));
		    dassert(loid == a_loid_array[i]);
		}
		{ // demo idiom for set or bag:
		    for (tmp_a_ref = o_ref->a_bag.get_elt(i=0);
			    tmp_a_ref != NULL;
			    tmp_a_ref = o_ref->a_bag.get_elt(++i))
		    { 
			SH_DO(tmp_a_ref.get_loid(loid));
			dassert(loid == a_loid_array[i]);
		    }
		}
	    }
	}
	SH_DO(SH_COMMIT_TRANSACTION);
    }

    destroy_obj(fname);
    destroy_obj("a_ref_junk");
    for(int i=0; i< NOBJS; i++) {
	destroy_obj(&names[i]);
    }

    delete local_setp;
    delete local_bagp;
    return 0;
}
