shore-mt
========

shore-mt (Scalable Heterogeneous Object REpository - MultiThreaded version), import of the 6.0.2 release of 03-Jan-2012  http://research.cs.wisc.edu/shore-mt/ ( differs from the DIAS version from http://diaswww.epfl.ch/shore-mt/ )



28 Oct 2012, Notes on compilation:
==================================

The code complains about gcc-4.4 not closing scopes correctly (see comments in shore-sm-6.0.2/src/fc/w_workaround.h:318).
While this was hard to believe, I used gcc-4.1.3 and g++-4.1.3 to get compilation.

I configured with

./configure --prefix=/home/you/pkg/shore-mt/wisc.edu.version/install --enable-dora

To get make check to succeed, I had to add in a couple of TCL_CVBUG casts in the src/sm/smsh directory.
