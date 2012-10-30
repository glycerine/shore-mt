/*$Header: /p/shore/shore_cvs/src/oo7/PartIdList.C,v 1.2 1994/02/04 19:20:30 nhall Exp $*/
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

#include <stdio.h>
#include "oo7.h"
#include "PartIdList.h"


// Add an id value to a list of part ids

void PartIdList::insert(int val)
{
    ListElem* elem = new ListElem;
    elem->value = val;
    elem->next = listPtr;
    listPtr = elem;
}


// Remove the first id value from a list of part ids

int PartIdList::remove()
{
    ListElem* firstElem = listPtr;
    int firstId = firstElem->value;
    listPtr = listPtr->next;
    delete firstElem;
    return firstId;
}


// Clear out an entire list of part ids

void PartIdList::clear()
{
    while (listPtr != NULL) {
        ListElem* firstElem = listPtr;
        listPtr = listPtr->next;
        delete firstElem;
    }
}

