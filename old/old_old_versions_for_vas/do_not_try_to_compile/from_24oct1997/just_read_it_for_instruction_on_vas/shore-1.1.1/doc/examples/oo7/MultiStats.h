
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


// structure for holding a single transaction timing result

class TimingRec {
public:
    double wall;
    double user;
    double system;
};


// class for managing multi-user transaction timing measurements

class MultiStats {
private:
    int count;		// number of measurements so far
    int limit;		// number of measurements allowed
    TimingRec* times;	// array of transaction timing results
public:
    MultiStats(int numXacts);
    void AddToStats(double wall, double user, double system);
    void PrintStats();
};

