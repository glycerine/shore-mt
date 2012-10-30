/*$Header: /p/shore/shore_cvs/src/oo7/PartIdSet.C,v 1.2 1994/02/04 19:20:34 nhall Exp $*/
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
#include "PartIdSet.h"

// Set all hash table entries to NULL

PartIdSet::PartIdSet()
{
    for (int h = 0; h < HashTableSize; h++) {
	hashTable[h] = NULL;
    }
    emptySet = 1;
}


// Reset all hash table entries to NULL (after deallocating contents)

void PartIdSet::clear()
{
    for (int h = 0; h < HashTableSize; h++) {
	Member* mem;
	Member* nextMem;
	mem = hashTable[h];
	hashTable[h] = NULL;
	while (mem != NULL) {
	    nextMem = mem->next;
	    delete mem;
	    mem = nextMem;
	}
    }
    emptySet = 1;
}


// Add an entry to a set of part ids

void PartIdSet::insert(int val)
{
    int h = hash(val);
    Member* mem  = new Member;
    mem->value   = val;
    mem->next    = hashTable[h];
    hashTable[h] = mem;
    emptySet 	 = 0;
}


// member function to look up a record in a hashed file based on its key

int PartIdSet::contains(int val)
{
    int h = hash(val);
    Member* mem = hashTable[h];
    while (mem != NULL && mem->value != val) { mem = mem->next; }
    return (mem != NULL);
}

