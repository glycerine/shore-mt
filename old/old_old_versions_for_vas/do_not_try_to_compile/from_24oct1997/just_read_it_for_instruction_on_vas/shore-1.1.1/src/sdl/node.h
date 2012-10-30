/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

struct node // my el-cheapo lisp-like cons cell
{
	short code; // type identifier
	// the code will in general specify the types of the other
	// fields.  At some point we should get rid of these.
	node * info;    // more data for this cell
	node * next;	// lisp cdr ~= next
	long lineno;	// for debugging.
	node(short c, node *inf, node *p2) ;
	int list_count(); // return list length,if it is alist.
	//void print_list(char *delim, int terminate=1, char *prefix=0);
	//void print_list2( char *prefix, char * sep, char * term);
	char * get_string();
	void error(char *); // print an error msg 
};

// and now, the persistent version
// give each node a shore oid field; we may want to move this
// into derived types later
// this should really be gotten from elsewhere.

// move the type member functions here
const int STR_TABSIZE = 1023; // some big primish number;
extern char * reserve_print_tab[/* LAST_TOKEN */];
struct rpair { char * str; short tok_num;};
extern void print_code(short code);
extern int strhash ( char *str2);
extern 
node *
string_tab[STR_TABSIZE]; // hash table for string nodes

