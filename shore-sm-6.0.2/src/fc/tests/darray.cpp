/* -*- mode:C++; c-basic-offset:4 -*-
     Shore-MT -- Multi-threaded port of the SHORE storage manager
   
                       Copyright (c) 2007-2009
      Data Intensive Applications and Systems Labaratory (DIAS)
               Ecole Polytechnique Federale de Lausanne
   
                         All Rights Reserved.
   
   Permission to use, copy, modify and distribute this software and
   its documentation is hereby granted, provided that both the
   copyright notice and this permission notice appear in all copies of
   the software, derivative works or modified versions, and any
   portions thereof, and that both notices appear in supporting
   documentation.
   
   This code is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. THE AUTHORS
   DISCLAIM ANY LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER
   RESULTING FROM THE USE OF THIS SOFTWARE.
*/
/*<std-header orig-src='shore' incl-file-exclusion='DARRAY_CPP'>

 $Id: darray.cpp,v 1.5 2012/01/02 21:52:22 nhall Exp $

SHORE -- Scalable Heterogeneous Object REpository

Copyright (c) 1994-99 Computer Sciences Department, University of
                      Wisconsin -- Madison
All Rights Reserved.

Permission to use, copy, modify and distribute this software and its
documentation is hereby granted, provided that both the copyright
notice and this permission notice appear in all copies of the
software, derivative works or modified versions, and any portions
thereof, and that both notices appear in supporting documentation.

THE AUTHORS AND THE COMPUTER SCIENCES DEPARTMENT OF THE UNIVERSITY
OF WISCONSIN - MADISON ALLOW FREE USE OF THIS SOFTWARE IN ITS
"AS IS" CONDITION, AND THEY DISCLAIM ANY LIABILITY OF ANY KIND
FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.

This software was developed with support by the Advanced Research
Project Agency, ARPA order number 018 (formerly 8230), monitored by
the U.S. Army Research Laboratory under contract DAAB07-91-C-Q518.
Further funding for this work was provided by DARPA through
Rome Research Laboratory Contract No. F30602-97-2-0247.

*/

#include <shore-config.h>
#include <w_base.h>
#include <dynarray.h>
#include <errno.h>
#include <sys/mman.h>
#include <algorithm>
#include <cstdlib>
#include <cassert>
#include <cstring>
#include <cstdio>

#include <unistd.h>
#include <cstdio>

static int foocount = 0;
struct foo {
    int id;
    foo() : id(++foocount) { std::fprintf(stdout, "foo#%d\n", id); }
    ~foo() { std::fprintf(stdout, "~foo#%d\n", id); }
};

template struct dynvector<foo>;

int main() {
    {
	dynarray mm;
	w_base_t::int8_t err;

#ifdef ARCH_LP64
#define BIGNUMBER 1024*1024*1024
#else
#define BIGNUMBER 1024*1024
#endif
	err = mm.init(size_t(5l*BIGNUMBER));
	// char const* base = mm;
	// std::fprintf(stdout, "&mm[0] = %p\n", base);
	err = mm.resize(10000);
	err = mm.resize(100000);
	err = mm.resize(1000000);
	err = mm.fini();
	w_assert0(!err);
    }
    {
		// test alignment
		dynarray mm;
		w_base_t::int8_t err =
		mm.init(size_t(5l*BIGNUMBER), size_t(BIGNUMBER));
		w_assert0(!err);
    }

    {
	int err;
	dynvector<foo> dv;
	err = dv.init(100000);
	std::fprintf(stdout, "size:%llu  capacity:%llu  limit:%llu\n", 
	(unsigned long long) dv.size(), (unsigned long long) dv.capacity(), (unsigned long long) dv.limit());
	err = dv.resize(4);
	std::fprintf(stdout, "size:%llu  capacity:%llu  limit:%llu\n", 
	(unsigned long long) dv.size(), (unsigned long long) dv.capacity(), (unsigned long long) dv.limit());
	err = dv.resize(10);
	std::fprintf(stdout, "size:%llu  capacity:%llu  limit:%llu\n", 
	(unsigned long long) dv.size(), (unsigned long long) dv.capacity(), (unsigned long long) dv.limit());
	foo f;
	err = dv.push_back(f);
	err = dv.push_back(f);
	err = dv.push_back(f);
	std::fprintf(stdout, "size:%llu  capacity:%llu  limit:%llu\n", 
	(unsigned long long) dv.size(), (unsigned long long) dv.capacity(), (unsigned long long) dv.limit());
	err = dv.resize(16);
	std::fprintf(stdout, "size:%llu  capacity:%llu  limit:%llu\n", 
	(unsigned long long) dv.size(), (unsigned long long) dv.capacity(), (unsigned long long) dv.limit());
	err = dv.fini();
	w_assert0(!err);
    }

    return 0;
}
