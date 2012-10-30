/*$Header: /p/shore/shore_cvs/src/oo7/BenchParams.h,v 1.2 1994/02/04 19:19:49 nhall Exp $*/
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


// parameters for controlling benchmark operations

    const int UpdateRepeatCnt = 4;	// repeat count for repeated updates

    const int Query1RepeatCnt = 10;	// repeat count for query #1 lookups

    const int Query2Percent = 1;	// selected % for query #2 lookups

    const int Query3Percent = 10;	// selected % for query #3 lookups

    const int Query4RepeatCnt = 10;	// repeat count for query #4 lookups

    const int NumNewCompParts = 10;	// # new composite parts for updates
    const int BaseAssmUpdateCnt = 10;	// # base assemblies used in updates

    const int MultiBenchMPL = 4;	// # multiuser benchmark processes
