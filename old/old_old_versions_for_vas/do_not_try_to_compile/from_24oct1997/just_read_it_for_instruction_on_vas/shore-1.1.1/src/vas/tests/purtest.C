/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#include <syslog.h>
#include <fcntl.h>
#include <iostream.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

main()
{
/*
	(void) openlog("shore", LOG_PID | LOG_CONS | LOG_NDELAY | LOG_NOWAIT, 
		LOG_LOCAL6);
	(void) closelog();
*/
	char buf[1000];
	int  fd;
	if((fd = open("/tmp/junk",O_RDONLY, 0)) <0) {
		cerr << "open" << endl;
		exit(1);
	}
	if(read(fd, buf, sizeof(buf))<0) {
		cerr << "open" << endl;
		exit(1);
	}

	for(int i=0; i<sizeof(buf); i++) {
		if(buf[i] != 'a') {
			cerr << i<<endl;
		}
	}
}
