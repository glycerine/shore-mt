// htmlignore

/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#if (__GNUC_MINOR__ < 7)

static char *rcsid="$Header: /p/shore/shore_cvs/src/examples/stree/cite.C,v 1.11 1996/07/19 22:56:59 nhall Exp $";

static void *const use_rcsid = (&use_rcsid, &rcsid, 0);
#endif

// htmlend
// Member functions of the Cite class
#include <iostream.h>
#include "stree.h"

void Cite::initialize(Ref<Document> d, long o) {
	doc = d;
	offset = o;
}

void Cite::print(long v) const {
	switch (v) {
		default:
		case 0: // just the file name
			cout << doc->get_name() << endl;
			break;
		case 1: // the file name and the corresponding line
			cout << doc->get_name() << ": ";
			doc->print_line(offset);
			break;
		case 2: // debugging version
			cout << "Cite, offset " << offset
				<< " in file " << doc->get_name()
				<< " cites";
			{
				int count = cites.get_size();
				for (int i = 0; i < count; i++) {
					Ref<Word> w = cites.get_elt(i);
					cout << " ";
					w->print(0);
				}
				cout << endl;
			}
			break;
	}
}

void Cite::finalize() {
	while (cites.delete_one()) {}
}
