/*$Header: /p/shore/shore_cvs/src/oo7/VarParams.h,v 1.3 1994/03/21 20:15:50 venkatar Exp $*/
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

extern REF(Pool) oo7_pool;

extern int NumAssmPerAssm;
extern int NumCompPerAssm;

extern int NumCompPerModule;
extern int NumAssmLevels;
extern int TotalModules;

extern int NumAtomicPerComp;
extern int NumConnPerAtomic;
extern int DocumentSize;
extern int ManualSize;

extern int TotalCompParts;
extern int TotalAtomicParts;
#ifdef MULTIUSER
extern int MultiSleepTime;
#endif
