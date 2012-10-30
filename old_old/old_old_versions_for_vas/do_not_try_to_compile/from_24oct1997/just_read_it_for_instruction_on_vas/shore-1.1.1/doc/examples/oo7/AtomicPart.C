/********************************************************/
/*                                                      */
/*               OO7 Benchmark                          */
/*                                                      */
/*              COPYRIGHT (C) 1993                      */
/*                                                      */
/*                Michael J. Carey 		        */
/*                David J. DeWitt 		        */
/*                Jeffrey Naughton 		        */
/*               Madison, WI U.S.A.                     */
/*                                                      */
/*	         ALL RIGHTS RESERVED                    */
/*                                                      */
/********************************************************/



////////////////////////////////////////////////////////////////////////////
//
// AtomicPart Methods
//
////////////////////////////////////////////////////////////////////////////

#include <libc.h>
#include <stdio.h>
#include <string.h>
#include "oo7.h"
#include "globals.h"
#include "VarParams.h"
#include "GenParams.h"
#include <sys/resource.h>
//#include <E/sm_btree.h>
#include "random.h"
#include "stats.h"

extern char*	types[NumTypes];

////////////////////////////////////////////////////////////////////////////
//
// AtomicPart Constructor
//
////////////////////////////////////////////////////////////////////////////

void
AtomicPart::init(long ptId, REF(CompositePart) cp)
{
    if (debugMode) {
        printf("AtomicPart::AtomicPart(ptId = %d)\n", ptId);
    }

    // initialize internal state of new part
    id = ptId;
    int typeNo = (int) (random() % NumTypes);
    strncpy(type, types[typeNo], (unsigned int)TypeSize);
    buildDate = MinAtomicDate +
        (int) (random() % (MaxAtomicDate-MinAtomicDate+1));
    x = (int) (random() % XYRange);
    y = (int) (random() % XYRange);
    // fill in a random document id (for query 9)
    docId = (int) random() % TotalCompParts + 1;

    // initialize space for connections to other parts
    // from = new (CompPartSet) Assoc(&CompPartSet);
    // from = from.new_persistent(CompPartSet);
    // from = new (CompPartSet) Assoc;
    // from.update()->init();

    // to = new (CompPartSet) Assoc(&CompPartSet);
    // to = to.new_persistent(CompPartSet);
    // to = new (CompPartSet) Assoc;
    // to.update()->init();

    partOf = cp;
    // AllParts.add(this);
    SH_DO(tbl->BuildDateIndex.insert(buildDate,this));
}

// destructor

void
AtomicPart::Delete()
{
    REF(Connection) cn;

    // remove part from extent of all atomic parts
    // AllParts.remove (this);

    // establish iterator over connected parts in "to" direction
    // and delete them.
    while (cn=to.delete_one())
    {
	 cn.update()->to = 0; // delete con. from other end.
	 cn.update()->from = 0;
	 SH_DO(cn.destroy());
    }

    // establish iterator over connected parts in "from" direction
    // and delete them
    while (cn=from.delete_one())
    {
	 cn.update()->from = 0; // delete con. from other end.
	 cn.update()->to = 0; // delete con. from other end.
	 SH_DO(cn.destroy());

    }

    SH_DO(tbl->BuildDateIndex.remove(buildDate,this));
    // finally, delete this object.
    SH_DO(this->get_ref().destroy());
}


////////////////////////////////////////////////////////////////////////////
//
// AtomicPart swapXY Method for use in update traversals
//
////////////////////////////////////////////////////////////////////////////

void AtomicPart::swapXY()
{
    if (debugMode) {
        printf("                    AtomicPart::swapXY(ptId = %d, x = %d, y = %d)\n", id,x,y);
    }

    // exchange X and Y values

    int tmp = (int)x;
    x = y;
    y = tmp;

    if (debugMode) {
        printf("                    [did swap, so x = %d, y = %d]\n", x, y);
    }
}


////////////////////////////////////////////////////////////////////////////
//
// AtomicPart toggleDate Method for use in update traversals
//
////////////////////////////////////////////////////////////////////////////

void AtomicPart::toggleDate()
{
    if (debugMode) {
        printf("                    AtomicPart::toggleDate(ptId = %d, buildDate = %d)\n",
	       id, buildDate);
    }

    // delete from index first
    SH_DO(tbl->BuildDateIndex.remove(buildDate,this));

    // increment build date if odd, decrement it if even

    if (buildDate % 2) {
	// odd case
	buildDate++;
    } else {
	// even case
	buildDate--;
    }

    // update the index
    SH_DO(tbl->BuildDateIndex.insert(buildDate,this));

    if (debugMode) {
        printf("                    [did toggle, so buildDate = %d]\n", buildDate);
    }
}


////////////////////////////////////////////////////////////////////////////
//
// AtomicPart DoNothing Method 
//
////////////////////////////////////////////////////////////////////////////

int RealWork; // set to one to make DoNothing do work.
int WorkAmount; // amount of work to do

void AtomicPart::DoNothing() const
{
    struct timeval tv;
    int ignore = 0;

    int i;

    if (id < 0) 
        printf("DoNothing: negative id.\n");

    if (debugMode) {
        printf("==> DoNothing(x = %d, y = %d, type = %s)\n", x, y, type);
    }

    if (RealWork) {
            for (i = 0; i < WorkAmount; i++) {
               gettimeofday(&tv, IGNOREZONE &ignoreZone);
            }
    }
}

