/*$Header: /p/shore/shore_cvs/src/oo7/baidlist.h,v 1.2 1994/02/04 19:21:24 nhall Exp $*/
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

// structure for keeping track of a list of base assemblies
// that reference a composite part as either a shared or private member

class BAListElem {
public:
    REF(BaseAssembly)  ba; // reference to a base assembly
    BAListElem*	next;
};

class BAIdList {
private:
    BAListElem* listPtr;
    BAListElem* current;

public:
    BAIdList() {listPtr = NULL; current=NULL;};
    void insert(REF(BaseAssembly) ba); // insert a new BA ref in list
    REF(BaseAssembly) next(); // scan list returning all elements
    void  initScan(); // scan list returning all elements
};

