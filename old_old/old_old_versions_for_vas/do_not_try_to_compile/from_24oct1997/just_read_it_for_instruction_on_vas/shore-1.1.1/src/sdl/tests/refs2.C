/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994,95,96 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#define MODULE_CODE
#include <iostream.h>
#include <fstream.h>
#include <std.h>
#include "ShoreApp.h"
#include "refs2.h"


Ref<Pool> pool;

# define dassert(ex)\
    {if (!(ex)){(void)fprintf(stderr,"Assertion failed: file \"%s\", line %d\n", __FILE__, __LINE__);exit(1);}}

void cleanup();

void is_this_dbo_valid (Ref<DBObject> db_reference )
{
	shrc    rc;
	bool    isres=false;
	LOID    loid;

	rc = db_reference.valid();
	if(rc) { 
		cerr << "not valid: " << rc << endl; 
	}
	rc = db_reference.get_loid(loid);
	if(rc) { 
		cerr << "no loid : " << rc << endl; 
	}
	rc = db_reference.is_resident(isres);
	cerr << (char *)(isres?"is":"is not") << " resident " << rc <<endl;
	rc = db_reference.fetch();
	if(rc) { 
		cerr << "not fetched: " << rc << endl; 
	}

}
void is_this_dbr_valid (Ref<DBReference> db_reference )
{
	shrc    rc;
	bool    isres=false;
	LOID    loid;

	rc = db_reference.valid();
	if(rc) { 
		cerr << "not valid: " << rc << endl; 
	}
	rc = db_reference.get_loid(loid);
	if(rc) { 
		cerr << "no loid : " << rc << endl; 
	}
	rc = db_reference.is_resident(isres);
	cerr << (char *)(isres?"is":"is not") << " resident " << rc <<endl;
	rc = db_reference.fetch();
	if(rc) { 
		cerr << "not fetched: " << rc << endl; 
	}

}




main (int argc, char **argv) 
{
	Ref<DBObject> a;
	Ref<DBObject> b;
	shrc rc;

    // Establish a connection with the vas and initialize 
    // the object  cache.
    SH_DO(Shore::init(argc, argv));

	cleanup();

    SH_BEGIN_TRANSACTION(rc);

    // If that failed, or if a transaction aborted...
    if(rc){
		// after longjmp
		cerr << rc << endl;
		return 1;
	} else {
		// The main body of the transaction goes here.

		SH_DO(Shore::chdir("/"));

		SH_DO(Ref<Pool>::create_pool("testp", 0755, pool));
		SH_DO(a.new_persistent(pool, a));
		SH_DO(b.new_persistent(pool, b));
		a.update()->addReference(1, b);

		cerr << "a->references_.get_size()=" <<
				a->references_.get_size() << endl;
		cerr << "a->referenced_by_.get_size()=" <<
				a->referenced_by_.get_size() << endl;
		is_this_dbr_valid (a->references_.get_elt(0) );
		cerr << "b->.references_.get_size()=" <<
				b->references_.get_size() << endl;
		cerr << "b->referenced_by_.get_size()=" <<
				b->referenced_by_.get_size() << endl;
		// is_this_dbr_valid (b->references_.get_elt(0) );
		SH_DO(SH_COMMIT_TRANSACTION);
	}
	cleanup();
}

void
cleanup()
{
	shrc 	rc;

    SH_BEGIN_TRANSACTION(rc);

    // If that failed, or if a transaction aborted...
    if(rc){
		// after longjmp
		cerr << rc << endl;
		return;
	} else {
		// The main body of the transaction goes here.

		SH_DO(Shore::chdir("/"));
		SH_DO(Ref<Pool>::lookup("testp", pool));
		SH_DO(pool.destroy_contents());
		SH_DO(Shore::unlink("testp"));

		SH_DO(SH_COMMIT_TRANSACTION);
	}
}

Ref<DBReference>
DBObject::addReference( long reftype, Ref<DBObject> obj )
{
  Ref<DBReference> dbref;
  dbref = new( pool ) DBReference;

  Ref<DBObject> myself = this;
  is_this_dbo_valid(myself);
  is_this_dbr_valid(dbref);
  is_this_dbo_valid(obj);

  dbref.update()->makeRelationship( myself, obj, reftype );
  is_this_dbr_valid(dbref);
  return dbref;
}


void 
DBReference::makeRelationship( Ref<DBObject> from, Ref<DBObject> to, long type  )
{
  /*
   * Set up the relationship.  The other half will be automatically
   * established by Shore's runtime system.
   */
  // type_ = type;
  refers_to_ = to;
  referred_by_ = from;
}



