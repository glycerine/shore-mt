// htmlignore

/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#if (__GNUC_MINOR__ < 7)
static char *rcsid="$Header: /p/shore/shore_cvs/src/examples/stree/document.C,v 1.13 1997/06/13 22:03:44 solomon Exp $";
static void *const use_rcsid = (&use_rcsid, &rcsid, 0);
#endif

// htmlend
// Member functions of the Document class
#include <iostream.h>
#include <string.h>
#include "stree.h"

void Document::initialize(char *base_name, long ilen) {
	body = 0;
	name = base_name;
	// set a char at the end of the body to initialze the
	// string space.
	body.set(ilen-1,0);
	// initialize cur_len.
	cur_len = 0;
}

// htmllabel stree:documentappendFUNC
void Document::append(char *str) {
	// body.set(str, body.length(), ::strlen(str));
	int str_size = ::strlen(str);
	body.set(str, cur_len, str_size);
	cur_len += str_size;
}

char *Document::get_name() const {
	return name;
}

long Document::size() const {
	// return body.strlen();
	return cur_len;
}

void Document::print_line(long offset) const {
	char buf[100];

	body.get(buf, offset, sizeof buf);
	buf[sizeof buf - 1] = 0;
	char *p = strchr(buf, '\n');
	if (p) *++p = 0;
	cout << buf;
}

// htmllabel stree:documentfinalizeFUNC
void Document::finalize() {
	Ref<Cite> p;
	while (p = cited_by.delete_one()) {
		p.update()->finalize();
		SH_DO(p.destroy());
	}
}
