#!/bin/sh
# --------------------------------------------------------------- #
# -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- #
# -- University of Wisconsin-Madison, subject to the terms     -- #
# -- and conditions given in the file COPYRIGHT.  All Rights   -- #
# -- Reserved.                                                 -- #
# --------------------------------------------------------------- #
# $Header: /p/shore/shore_cvs/src/examples/stree/run_tests.sh,v 1.2 1997/06/13 22:03:45 solomon Exp $

run_cmd() { echo === "$*"; echo "$*" >>$out; $* >>$out; }
run_all() {
	echo ======== $1 ========= >>$out
	run_cmd $1 -aV test\?
	run_cmd $1 -lV six
	run_cmd $1 -lV eight
	run_cmd $1 -dV test2
	run_cmd $1 -lV six
	run_cmd $1 -dV test1
	run_cmd $1 -lV seven
	run_cmd $1 -p
	run_cmd $1 -dV test3
	run_cmd $1 -p
	run_cmd $1 -aV sonnets/sonnet01\?
	run_cmd $1 -lV summers
	run_cmd $1 -l summers
	run_cmd swc $2 summers
	run_cmd sedit $2 summers < /dev/null
	run_cmd $1 -d sonnet010 sonnet011 sonnet012 sonnet013 sonnet014 \
sonnet015 sonnet016 sonnet017 sonnet018 sonnet019
}

fname=runtest
out=$fname.out.$$

trap "rm -f $out" 2

cp /dev/null $out

if test $# -eq 0
then
	run_all stree
	run_cmd stree -c
	run_all doc_index -i
elif test stree = "$1"
then
	run_all stree
	run_cmd stree -c
elif test doc_index = "$1"
then
	run_all doc_index -i
else
	echo "usage: $0 [stree | doc_index]"
	exit 1
fi

if diff -c $fname.log $out
then
	echo $fname test successfully executed
	rm -f $out
	exit 0
else
	mv -f $out $fname.bad
	echo =========== Error: $fname test failed
	echo correct results are in $fname.log
	echo observed results are in $fname.bad
	exit 1
fi
