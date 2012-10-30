#ifndef PARTID2_H
#define PARTID2_H

/*$Header: /p/shore/shore_cvs/src/oo7/PartIdList.h,v 1.2 1994/02/04 19:20:32 nhall Exp $*/
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

class ListElem {
public:
    int		value;
    ListElem*	next;
};

class PartIdList {
private:
    ListElem* listPtr;

public:
    PartIdList() { listPtr = NULL; };
    void insert(int val);
    int  remove();
    void clear();
    int	 empty() { return listPtr == NULL; };
};

#endif

