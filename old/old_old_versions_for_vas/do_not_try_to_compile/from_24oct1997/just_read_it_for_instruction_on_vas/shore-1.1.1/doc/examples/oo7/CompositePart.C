/*$Header: /p/shore/shore_cvs/src/oo7/CompositePart.C,v 1.19 1995/07/17 15:50:17 nhall Exp $*/
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
// CompositePart Methods
//
////////////////////////////////////////////////////////////////////////////

#include <libc.h>
#include <stdio.h>

#include "oo7.h"
#include "globals.h"
#include "GenParams.h"
#include "VarParams.h"
//#include "IntBtree.h"
//#include "StringBtree.h"
#include "baidlist.h"
#include "random.h"
#include <string.h>

extern char*	types[NumTypes];

extern BAIdList* private_cp;
extern BAIdList* shared_cp;

////////////////////////////////////////////////////////////////////////////
//
// CompositePart Constructor
//
////////////////////////////////////////////////////////////////////////////

void
CompositePart::init(long cpId)
{
    // int 	partDate;
    int 	i, from, to;
    long int 	atomicid /* , length */ ;
#ifdef __GNUG__
    REF(AtomicPart)	atomicParts[NumAtomicPerComp];
#else
    REF(AtomicPart)	*atomicParts;
#endif
    REF(Connection)	cn;
    REF(BaseAssembly) ba;
    char	title[TitleSize];

#ifndef __GNUG__
    atomicParts = new REF(AtomicPart)[NumAtomicPerComp];
#endif

    if (debugMode) {
        printf("CompositePart::CompositePart(cpId = %d)\n", cpId);
    }

    // initialize the simple stuff
    id = cpId;
    int typeNo = (int) (random() % NumTypes);
    strncpy(type, types[typeNo], (unsigned int)TypeSize);

    // for the build date, decide if this part is young or old, and then
    // randomly choose a date in the required range

    if (cpId % YoungCompFrac == 0) {
        // young one
        if (debugMode) {
            fprintf(stderr, "(young composite part, id = %d.)\n", id);
        }
        buildDate = MinYoungCompDate +
                    (int)(random() % (MaxYoungCompDate - MinYoungCompDate + 1));
    } else {
        // old one
        if (debugMode) {
            fprintf(stderr, "(old composite part, id = %d.)\n", id);
        }
        buildDate = MinOldCompDate +
                    (int)(random() % (MaxOldCompDate - MinOldCompDate + 1));
    }


    // initialize the documentation (indexed by its title and id) ...
    sprintf(title, "Composite Part %08d", cpId);
    // documentation = new (DocumentSet) Document(cpId, this);
    // documentation = documentation.new_persistent(DocumentSet);
    documentation = new (DocumentSet) Document;
    documentation.update()->init(cpId,this);

    // insert title into document index
    W_COERCE(tbl->DocumentIdx.insert(title, documentation));

    // insert id into document id index
    W_COERCE(tbl->DocumentIdIdx.insert(cpId, documentation));
#ifdef PARSETS
    nextAtomicId = cpId*NumAtomicPerComp;
#endif
    // now create the atomic parts (indexed by their ids) ...
    for (i = 0; i < NumAtomicPerComp; i++) 
    {
	atomicid = nextAtomicId + i;

	// create a new atomic part
	// the AtomicPart constructor takes care of setting up
	// the back pointer to the containing CompositePart()
	// atomicParts[i] = new (CompPartSet) AtomicPart(atomicid,this);
	// now in its own collection
	// atomicParts[i] = new (AtomicPartSet) AtomicPart(atomicid,this);
	// atomicParts[i] = atomicParts[i].new_persistent(AtomicPartSet);
	atomicParts[i] = new (AtomicPartSet) AtomicPart;
	atomicParts[i].update()->init(atomicid,this);

	// stick the id of the part into the index
	W_COERCE(tbl->AtomicPartIdx.insert(atomicid, atomicParts[i]));

	// connect the atomic part to the composite part
	parts.add(atomicParts[i]);

	// first atomic part is the root part
	if (i == 0) rootPart = atomicParts[i];
    }

    // ... and then wire them semi-randomly together (as a ring plus random
    // additional connections to ensure full part reachability for traversals)

    for (from = 0; from < NumAtomicPerComp; from++) {
	for (i = 0; i < NumConnPerAtomic; i++) {
	    if (i == 0) {
	        to = (from + 1) % NumAtomicPerComp;
	    } else {
	        to = (int) (random() % NumAtomicPerComp);
	    }
	    // cn = new (CompPartSet) Connection(atomicParts[from], atomicParts[to]);
	    // cn = cn.new_persistent(CompPartSet);
	    cn = new (CompPartSet) Connection;
	    cn.update()->init(atomicParts[from],atomicParts[to]);
	}
    }
    nextAtomicId += NumAtomicPerComp;

    // finally insert this composite part as a child of the base
    // assemblies that use it

    // first the assemblies using the comp part as a shared component
    shared_cp[cpId].initScan();

    // get the first base assembly
    ba = shared_cp[cpId].next();
    while (ba != NULL)
    {
	// add this assembly to the list of assemblies in which
	// this composite part is used as a shared member
	usedInShar.add(ba);

	// then add the composite part cp to the list of shared parts used
	// in this assembly
	ba.update()->componentsShar.add(this);

	// continue with next base assembly
        ba = shared_cp[cpId].next();
    }

    // next the assemblies using the comp part as a private component
    private_cp[cpId].initScan();

    // get the first base assembly
    ba = private_cp[cpId].next();
    while (ba != NULL)
    {
	// first add this assembly to the list of assemblies in which
	// this composite part is used as a private member
	usedInPriv.add(ba);

	// then add the composite part cp to the list of private parts used
	// in the assembly
	ba.update()->componentsPriv.add(this);

	// continue with next base assembly
        ba = private_cp[cpId].next();
    }

#ifndef __GNUG__
    delete [] atomicParts;
#endif

}

// destructor
void
CompositePart::Delete()
{
    REF(BaseAssembly) baH;
    REF(AtomicPart)	  apH;
    REF(AtomicPart)	  apH2;
    REF(Document) 	docH;
    int nrm;
    char	docTitle[40];

    strcpy(docTitle,documentation->title);
    if(  tbl->DocumentIdx.remove(docTitle,documentation))
      printf("error deleting document from title index in CompPart destructor\n");
    if ( tbl->DocumentIdIdx.remove(documentation->id,documentation))
      printf("error deleting document from id index in CompPart destructor\n");
    
    // delete documentation; 
    //documentation.destroy();
    documentation.update()->Delete();

    // walk through usedInPriv set deleting all backward references
    // to this composite part from base assemblies of which it is a part
    int i;
    for(i= 0; i<usedInPriv.get_size(); ++i)
    {
	baH =usedInPriv.get_elt(i);
	baH.update()->componentsPriv.del(this);
    }

    // walk through usedInShar set deleting all backward references
    // to this composite part from base assemblies of which it is a part
    for(i= 0; i<usedInShar.get_size(); ++i)
    {
	baH =usedInShar.get_elt(i);
	baH.update()->componentsShar.del(this);
    }

    // delete all atomic parts that compose this composite part
    for(i= 0; i<parts.get_size(); ++i)
    {
	apH = parts.get_elt(i);
	W_COERCE(tbl->AtomicPartIdx.remove(apH->id,apH));
	apH.update()->Delete();
    }

    // dts 3-11-93 AllCompParts is not used, so don't maintain it.
    // (used in Reorg code though, but that's not called currently)
    // AllCompParts.remove(this);  // finally remove from the extent
}
