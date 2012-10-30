// htmlignore

/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#if (__GNUC_MINOR__ < 7)
static char *rcsid="$Header: /p/shore/shore_cvs/src/examples/stree/ix_word.C,v 1.5 1996/07/19 22:57:15 nhall Exp $";
static void *const use_rcsid = (&use_rcsid, &rcsid, 0);
#endif

// htmlend
// Member functions of the Word class
#include <iostream.h>
#include <string.h>
#include "doc_index.h"

extern Ref<Pool> nodes;
extern Ref<DocIndex> repository;

// htmllabel stree:wordinitializeFUNC
void Word::initialize(char *word) {
	value = word;
}

long Word::count() const {
	return cited_by.get_size();
}

// htmllabel stree:wordoccurrenceFUNC
Ref<Cite> Word::occurrence(long i) const {
	return cited_by.get_elt(i);
}

// htmllabel stree:wordoccursonFUNC
void Word::occurs_on(Ref<Cite> cite) {
	cited_by.add(cite);
}

void Word::finalize() {
	repository.update()->delete_word(value);
}

void Word::print(long verbose) const {
	if (verbose) {
		int s = cited_by.get_size();
		cout << "Word '" << (char *)value
			<< "' occurs on " << s << " line" << (s==1 ? "" : "s") << endl;
	}
	else cout << (char *)value;
}
