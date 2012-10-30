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
#include "index_vars.h"

void
usage(ostream &/*os*/, char */*progname*/)
{
    exit(1);
}

#define PERROR(rc) { if(rc && rc.err_num()!=SH_NotFound) {\
	cerr << __LINE__ << " " <<__FILE__<<":"<<endl;\
	cerr << rc << endl; if(rc = RCOK);\
	} }

void
scanboth(REF(IndexObj)w)
{
    Ref<Person> p;
    shrc rc;

    {
	index_iter<typeof(w->name_index)> iter(w->name_index); // -inf to +inf
	cerr << "Scan by name. " << endl;
	for ( rc = iter.next();
		rc == RCOK && !iter.eof;
		rc = iter.next() ) {

	    p = iter.cur_val;
	    if(p) {
		cerr << "key=" << iter.cur_key << " person=" << p->name << " age " << p->age << endl;
	    } else {
		cerr << "key=" << iter.cur_key << " no such person" << endl;
	    }
	}
	PERROR(rc);
	rc = iter.close();
	PERROR(rc);
    }

	// use other implementation
    {
	IndexScanIter<long,Ref<Person> >	iter(w->age_index); // -inf to +inf
	cerr << "Scan by age. " << endl;
	while( 
		!(rc = iter.next()) 
		&&  
		!iter.eof 
	) { 
	    p = iter.cur_val;
	    if(p) {
		cerr << "key=" << iter.cur_key << " person=" << p->name << " age " << p->age << endl;
	    } else {
		cerr << "key=" << iter.cur_key << " no such person" << endl;
	    }
	}
	PERROR(rc);
	rc = iter.close();
	PERROR(rc);
    }
}

void
partialscans(REF(IndexObj)w)
{
    Ref<Person> p;
    shrc rc;

    {
	IndexScanIter<sdl_string,Ref<Person> >	iter(w->name_index); // -inf to +inf
	// change scan conditions from -inf to "c"
	iter.SetLowerCond(geNegInf);
	iter.SetUB("c");
	iter.SetUpperCond(leOp);

	cerr << "Partial scan of names: beginning through c. " << endl;
	for ( rc = iter.next();
		rc == RCOK && !iter.eof;
		rc = iter.next() ) {

	    p = iter.cur_val;
	    if(p) {
		cerr << "key=" << iter.cur_key << " person=" << p->name << " age " << p->age << endl;
	    } else {
		cerr << "key=" << iter.cur_key << " no such person" << endl;
	    }
	}
	PERROR(rc);
	rc = iter.close();
	PERROR(rc);
    }

    {
	IndexScanIter<long,Ref<Person> >	iter(w->age_index); // -inf to +inf
	// change scan conditions from 3 through end
	iter.SetLowerCond(geOp);
	iter.SetLB(3);
	iter.SetUpperCond(lePosInf);

	cerr << "Partial scan of ages: 3 through end. " << endl;
	while( 
		!(rc = iter.next()) 
		&&  
		!iter.eof 
	) { 
	    p = iter.cur_val;
	    if(p) {
		cerr << "key=" << iter.cur_key << " person=" << p->name << " age " << p->age << endl;
	    } else {
		cerr << "key=" << iter.cur_key << " no such person" << endl;
	    }
	}
	PERROR(rc);
	rc = iter.close();
	PERROR(rc);
    }
}
int
main(int argc, char *argv[])
{
    char 	*progname = argv[0];
    char 	*fname = "index_vars_pool";
    shrc 	rc;
    Ref<Pool> pool;

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
	// create an instance of IndexObj with Btrees
	/////////////////////////////////////////////////
	{
	    Ref<IndexObj> w;
	    Ref<Person> p;

	    SH_DO( REF(Pool)::create(fname, 0755, pool) );
	    SH_DO( REF(IndexObj)::new_persistent (pool, w) ) ;

	    if(!w) {
		cerr << "Cannot create " << fname << endl;
	    } else {
		rc= w->name_index.init(UniqueBTree);
		PERROR(rc);

		rc= w->age_index.init(BTree);
		PERROR(rc);
	    }
	    cerr << fname << " created. " << endl;

	    cerr << "Scanning empty indexes " << endl; scanboth(w);
		cerr << endl;

	    {
		// create a few instances of Person
		cerr << "Adding goofy" << endl;

		SH_DO( REF(Person)::new_persistent (pool, p) ) ;
		p.update()->name = "goofy";
		p.update()->age = 3;
		rc = w->name_index.insert(p->name, p);
		PERROR(rc);
		rc = w->age_index.insert(p->age, p);
		PERROR(rc);
		cerr << "Added goofy" << endl; scanboth(w); cerr << endl;


		cerr << "Adding daffy" << endl; 
		SH_DO( REF(Person)::new_persistent (pool, p) ) ;
		p.update()->name = "daffy";
		p.update()->age = 4;
		PERROR(rc);
		rc = w->name_index.insert(p->name, p);
		PERROR(rc);
		rc = w->age_index.insert(p->age, p);
		PERROR(rc);
		cerr << "Added daffy" << endl; scanboth(w); cerr << endl;

		cerr << "Adding bugs" << endl; 
		SH_DO( REF(Person)::new_persistent (pool, p) ) ;
		p.update()->name = "bugs";
		p.update()->age = 6;
		PERROR(rc);
		rc = w->name_index.insert(p->name, p);
		PERROR(rc);
		rc = w->age_index.insert(p->age, p);
		PERROR(rc);
		cerr << "Added bugs" << endl; scanboth(w); cerr << endl;

		cerr << "Adding donald" << endl; 
		SH_DO( REF(Person)::new_persistent (pool, p) ) ;
		p.update()->name = "donald";
		p.update()->age = 1;
		PERROR(rc);
		rc = w->name_index.insert(p->name, p);
		PERROR(rc);
		rc = w->age_index.insert(p->age, p);
		PERROR(rc);
	    }
		cerr << "Added donald" << endl; scanboth(w); cerr << endl;

	    cerr << "persons created and inserted " << endl;
	    scanboth(w);
	    {
		// locate a person by name, another by age
		cerr << "Locate by name. " << endl;

		bool	found=false;
		rc = w->name_index.find("donald", p, found);
		PERROR(rc);
		if(found && p) {
		    cerr << "key=" << "donald" << " person=" << p->name << " age " << p->age << endl;
		} else {
		    cerr << "Noone named " << "donald" <<endl;
		}

		cerr << "Locate by age. " << endl;
		found = false;
		rc = w->age_index.find(4, p, found);
		PERROR(rc);
		if(found && p) {
		    cerr << "key=" << 4 << " person=" << p->name << " age " << p->age << endl;
		} else {
		    cerr << "Noone aged " << 4 <<endl;
		}
	    }
	    cerr << "Remove daffy. " << endl;
	    {

		// remove someone
		int	num;
		rc = w->name_index.remove("daffy", num);
		PERROR(rc);
		cerr << "removed " << num << " instance(s) of daffy from name index" << endl;
		{
		    Ref<Person> 	s;
		    bool		found=false;
		    rc = w->age_index.find(4, s, found);
		    PERROR(rc);

		    rc = w->age_index.remove(4, s);
		    PERROR(rc);
		    cerr << "removed 4 from age index" << endl;
		}
	    }
	    cerr << "Rescan. " << endl;
	    scanboth(w);
	    partialscans(w);

	}
	SH_DO(SH_COMMIT_TRANSACTION);
    }

    SH_BEGIN_TRANSACTION(rc);
    if(rc){
	// after longjmp
	cerr << rc << endl;
	return 1;
    } else {
	SH_DO(pool.destroy_contents());
	SH_DO(Shore::unlink(fname));
	SH_DO(SH_COMMIT_TRANSACTION);
    }
    return 0;
}
