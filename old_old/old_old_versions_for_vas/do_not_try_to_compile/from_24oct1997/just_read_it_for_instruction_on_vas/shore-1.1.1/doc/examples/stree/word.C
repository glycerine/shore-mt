// htmlignore

/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#if (__GNUC_MINOR__ < 7)
static char *rcsid="$Header: /p/shore/shore_cvs/src/examples/stree/word.C,v 1.12 1996/07/19 22:57:22 nhall Exp $";
static void *const use_rcsid = (&use_rcsid, &rcsid, 0);
#endif

// htmlend
// Member functions of the Word class
#include <iostream.h>
#include <string.h>
#include "stree.h"

extern Ref<Pool> nodes;

// htmllabel stree:wordinitializeFUNC
void Word::initialize(char *word) {
	value = word;
	left = NULL;
	right = NULL;
}

long Word::count() const {
	return cited_by.get_size();
}

// htmllabel stree:wordoccurrenceFUNC
Ref<Cite> Word::occurrence(long i) const {
	return cited_by.get_elt(i);
}

Ref<Word> Word::find_or_add(char *s) {
	int i = strcmp(s,value);
	if (i == 0)
		return this;
	if (i < 0) {
		if (left) return left.update()->find_or_add(s);
		else {
			left = new(nodes) Word;
			left.update()->initialize(s);
			return left;
		}
	}
	else {
		if (right) return right.update()->find_or_add(s);
		else {
			right = new(nodes) Word;
			right.update()->initialize(s);
			return right;
		}
	}
}

Ref<Word> Word::find(char *s) const {
	int i = strcmp(s,value);
	if (i == 0) return this;
	if (i < 0) return left ? left->find(s) : (Ref<Word>)NULL;
	return right ? right->find(s) : (Ref<Word>)NULL;
}

// htmllabel stree:wordoccursonFUNC
void Word::occurs_on(Ref<Cite> cite) {
	cited_by.add(cite);
}

void Word::print(long verbose) const {
	if (verbose) {
		int s = cited_by.get_size();
		cout << "Word '" << (char *)value
			<< "' occurs on " << s << " line" << (s==1 ? "" : "s") << endl;
	}
	else cout << (char *)value;
}
