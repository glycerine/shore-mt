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



Info and copyright notices from the Shore-MT home page 
(http://research.cs.wisc.edu/shore-mt/) 
-------------------------------------------------------

<h1>SHORE Storage Manager: The Multi-Threaded Version</h1>

<p>
Description</a></h2>
This is an experiment test-bed library for use by researchers who wish to write multi-threaded software that manages persistent data.<p>
This storage engine provides the following capabilities:<ul>
<li>transactions with ACID properties, with ARIES-based logging and recovery, primitives for partial rollback, transaction chaining, and early lock release,</li><li>prepared-transaction support for two-phased commit,</li><li>persistent storage structures : B+ tree indexes, R* trees (spatial indexes), and files of untyped records,</li><li>fine-grained locking for records and B+ tree indexes with deadlock detection, optional lock escalation and optional coarse-grained locking,</li><li>in-memory buffer management with optional prefetching,</li><li>extensible statistics-gathering, option-processing, and error-handling facilities.</li></ul>
<p>
This software runs on Pthreads, thereby providing its client software (e.g., a database server) multi-threading capabilities and resulting scalability from modern SMP and NUMA architectures, and has been used on Linux/x86-64 and Solaris/Niagara architectures.
<hr>
<h2><a class="anchor" name="Background">
Background</a></h2>
The SHORE (Scalable Heterogeneous Object REpository) project at the University of Wisconsin - Madison Department of Computer Sciences produced the first release of this storage manager as part of the full SHORE release in 1996. The storage manager portion of the SHORE project was used by other projects at the UW and elsewhere, and was intermittently maintained through 2008.<p>
The SHORE Storage Manager was originally developed on single-cpu Unix-based systems, providing support for "value-added" cooperating peer servers, one of which was the SHORE Value-Added Server (<a href="http://research.cs.wisc.edu/shore">http://research.cs.wisc.edu/shore</a>), and another of which was Paradise (<a href="http://research.cs.wisc.edu/paradise">http://research.cs.wisc.edu/paradise</a>) at the University of Wisconsin. The TIMBER (<a href="http://www.eecs.umich.edu/db/timber">http://www.eecs.umich.edu/db/timber</a>) and Pericope (<a href="http://www.eecs.umich.edu/periscope">http://www.eecs.umich.edu/periscope</a>) projects at the University of Michigan, PREDATOR (<a href="http://www.distlab.dk/predator">http://www.distlab.dk/predator</a>) at Cornell and Lachesis (<a href="http://www.vldb.org/conf/2003/papers/S21P03.pdf">http://www.vldb.org/conf/2003/papers/S21P03.pdf</a>) used the SHORE Storage Manager. The storage manager has been used for innumerable published studies since then.<p>
The storage manager had its own "green threads" and communications layers, and until recently, its code structure, nomenclature, and contents reflected its SHORE roots.<p>
In 2007, the 
<A HREF="http://dias.epfl.ch/">
Data-Intensive Applications and Systems Lab (DIAS)
</A>
at 
<A HREF="http://epfl.ch/">
Ecole Polytechnique Federale de Lausanne
</A>
began work on a port of release 5.0.1 of the storage manager to Pthreads, and developed more scalable synchronization primitives, identified bottlenecks in the storage manager, and improved the scalability of the code. This work was on a Solaris/Niagara platform and was released as Shore-MT <a href="http://diaswww.epfl.ch/shore-mt">http://diaswww.epfl.ch/shore-mt</a>). It was a partial port of the storage manager and did not include documentation. Projects using Shore-MT include StagedDB/CMP (<a href="http://www.cs.cmu.edu/~stageddb/">http://www.cs.cmu.edu/~stageddb/</a>), DORA (<a href="http://www.cs.cmu.edu/~ipandis/resources/CMU-CS-10-101.pdf">http://www.cs.cmu.edu/~ipandis/resources/CMU-CS-10-101.pdf</a>)<p>
In 2009, the University of Wisconsin - Madison took the first Shore-MT release and ported the remaining code to Pthreads. This work as done on a Red Hat Linux/x86-64 platform. This release is the result of that work, and includes this documentation, bug fixes, and supporting test code. In this release some of the scalability changes of the DIAS release have been disabled as bug work-arounds, with the hope that further work will improve scalability of the completed port.
<hr>
<h2><a class="anchor" name="Copyrights">
Copyrights</a></h2>
This distribution contains code and documentation subject to one or more of the following copyrights.<p>
The main code base of the storage manager is subject to the SHORE/UW copyright (given below) and most of it is also subject to the SHORE-MT/DIAS copyright (also given below). Both copyrights are hereby extended to the date of this release, 2010.<p>
The atomic operations library is taken from the OPENSOLARIS release and is subject to Sun Microsystems copyright, and to the OPENSOLARIS license, found in src/atomic_ops/OPENSOLARIS.LICENSE. It is lengthy and so it is not included here.<p>
The strstream compatibility code 
is subject to the Silicon Graphics copyright, below.<p>
The regex code 
found in the src/common/ library 
is subject to the Henry Spencer/ATT copyright and license, contained in src/common/regex2.h, 
and included below.<p>
What little remains of the old SHORE sthreads library is subject to copyright given in those source files (src/sthread/sthread.h as well as to the SHORE/UW and SHORE-MT/DIAS copyrights.<p>
<ul>
<li><b>SHORE/UW</b> <b>Copyright:</b> </li></ul>
<p>
SHORE -- Scalable Heterogeneous Object REpository<p>
Copyright (c) 1994-2010 Computer Sciences Department, University of Wisconsin -- Madison All Rights Reserved.<p>
Permission to use, copy, modify and distribute this software and its documentation is hereby granted, provided that both the copyright notice and this permission notice appear in all copies of the software, derivative works or modified versions, and any portions thereof, and that both notices ape University of California.<p>
Permission is granted to anyone to use this software for any purpose on any computer system, and to alter it and redistribute it, subject to the following restrictions:<p>
1. The author is not responsible for the consequences of use of this software, no matter how awful, even if they arise from flaws in it.<p>
2. The origin of this software must not be misrepresented, either by explicit claim or by omission. Since few users ever read sources, credits must appear in the documentation.<p>
3. Altered versions must be plainly marked as such, and must not be misrepresented as being the original software. Since few users ever read sources, credits must appear in the documentation.<p>
4. This notice may not be removed or altered.
<HR>
<h2><a class="anchor" name="Downloads">
Downloads</a></h2>
<p>

<LI><A HREF="http://research.cs.wisc.edu/shore-mt/ftp/">
Source and documentation(-dox) tarballs can be downloaded via HTTP from here.
</A> 


<LI><A HREF="http://research.cs.wisc.edu/shore-mt/onlinedoc/html/index.html">
Documentation of latest release is on-line here.
</A> 

<P>
See also:
<P>
<A HREF="http://diaswww.epfl.ch/">
WWW at Data-Intensive Applications and Systems Lab (DIAS)
</A>
<P>
<A HREF="http://www.cs.wisc.edu/">
UW-Madison, CS Department Server
</A>
