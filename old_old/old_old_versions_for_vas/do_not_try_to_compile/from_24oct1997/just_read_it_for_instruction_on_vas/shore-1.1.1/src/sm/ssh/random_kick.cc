/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994,95,96 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#include <sys/types.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream.h>


extern "C" {
	long random();
	int srandom(int);
	char *initstate(unsigned, char*, int);
	char *setstate(char *);
};

int rseed=1;
char rstate[32]= {
    0x76, 0x4, 0x24, 0x2c,
    0x03, 0xab, 0x38, 0xd0,
    0xab, 0xed, 0xf1, 0x23,
    0x03, 0x00, 0x08, 0xd0,
    0x76, 0x40, 0x24, 0x2c,
    0x03, 0xab, 0x38, 0xd0,
    0xab, 0xed, 0xf1, 0x23,
    0x01, 0x00, 0x38, 0xd0
};

int
dorandom(long mod) 
{
    long res = random();

    if(mod==0) {
	(void) setstate(rstate);
	srandom(rseed);
    } else if(mod>0) {
	res %= mod;
    }
    return (int) res;
}

void
die(
    int 
)
{
    cout << flush;
    exit(158);
}

main(
	int ac,
	char *av[]
) 
{
    if (ac != 6) {
	cerr << "Usage: "
	<< av[0] 
	<< " <sig> <pid> <max0> <max1> <max2> " 
	<< endl;
	return 1;
    }

    signal(SIGUSR1, SIG_IGN);
    signal(SIGUSR2, die);

    int pid, sig;
    int max0 = atoi(av[3]);
    int max1 = atoi(av[4]);
    int max2 = atoi(av[5]);
    sig = atoi(av[1]);
    pid = atoi(av[2]);

    int category=0, t=0;
    while(1) {
	pid_t pp = getppid();
	if(pp == 1) { 
	    cerr << "random_kick: parent went away -- exiting " << endl;
	    return 1;
	}
      
	category = dorandom(3);

	switch(category) {
	case 0:
	     t=dorandom(max0);
	     break;
	case 1:
	     t=dorandom(max1);
	     break;
	case 2:
	     t=dorandom(max2);
	     break;
	}
	cout << "random_kick: pid " << getpid() << " sleep... " << t << endl;
	if(t>0) {
	    sleep(t);
	}
	cout << "random_kick: kill " << pid << "," << sig << endl;
	kill(pid, sig);
    }
}

