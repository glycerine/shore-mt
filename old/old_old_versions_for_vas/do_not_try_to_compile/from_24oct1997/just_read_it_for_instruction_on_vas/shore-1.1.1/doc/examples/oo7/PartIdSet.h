#ifndef PARTID_H
#define PARTID_H

/*$Header: /p/shore/shore_cvs/src/oo7/PartIdSet.h,v 1.2 1994/02/04 19:20:37 nhall Exp $*/

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



#define HashTableSize 511

class Member {
public:
    int		value;
    Member*	next;
};

class PartIdSet {
private:
    Member* hashTable[HashTableSize];
    int     emptySet;
    int hash (int val) 
    { return((unsigned) ((unsigned) val * 12345 + 6789) % HashTableSize); }

public:
    PartIdSet();
    ~PartIdSet() { clear(); };
    void clear();
    void insert(int val);
    int  contains(int val);
    int	 empty() { return(emptySet); };
};

#endif

