/*$Header: /p/shore/shore_cvs/src/oo7/baidlist.C,v 1.2 1994/02/04 19:21:21 nhall Exp $*/
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

#include <libc.h>
#include <stdio.h>
#include <stdlib.h>
#include "oo7.h"
#include "baidlist.h"

// Add a Base assembly reference to a list 

void BAIdList::insert(REF(BaseAssembly) value)
{
    BAListElem* elem = new BAListElem;
    if (elem == NULL) 
    {
    	printf("new failed in BAIdlist.insert()\n");
	return;
    }
    elem->ba = value;
    elem->next = listPtr;
    listPtr = elem;
};

// initialize a scan
void BAIdList::initScan()
{
    current = listPtr;
};

// Scan 
REF(BaseAssembly) 
BAIdList::next()
{
    REF(BaseAssembly) ba;

    if (current == NULL) return NULL;
    else
    {
	ba = current->ba;
	current = current->next;
	return ba;
    }
};


