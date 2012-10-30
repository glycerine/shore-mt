/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994,95,96 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */


extern "C" {
	const char *ssh_err_msg(const char *str);
	unsigned int ssh_err_code(const char *x);
	const char *ssh_err_name(unsigned int x);
}
