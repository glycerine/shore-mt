/*$Header: /p/shore/shore_cvs/src/oo7/Connection.C,v 1.8 1995/03/24 23:57:50 nhall Exp $*/
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
// Connection Methods
//
////////////////////////////////////////////////////////////////////////////

#include <libc.h>
#include <stdio.h>
#include "oo7.h"
#include "globals.h"
#include "GenParams.h"
#include "random.h"
#include <string.h>

extern char*	types[NumTypes];

////////////////////////////////////////////////////////////////////////////
//
// Connection Constructor
//
////////////////////////////////////////////////////////////////////////////

void
Connection::init(REF(AtomicPart) fromPart, REF(AtomicPart) toPart)
{
    //int size;

    if (debugMode) {
        printf("Connection::Connection(fromId = %d, toId = %d)\n",
            fromPart->id, toPart->id);
    }

    // initialize the simple stuff
    int typeNo = (int) (random() % NumTypes);
    strncpy(type, types[typeNo], (unsigned int)TypeSize);
    length = (int) (random() % XYRange);

    from = fromPart;  // establish pointer back to "from" part 
    to = toPart;      // establish forward pointer to "to" part
    // if not using relationship, this would establish back pointers.
    // fromPart.update()->to.add( this); // establish pointer to connection
    // toPart.update()->from.add( this); // establish pointer to connection
}
