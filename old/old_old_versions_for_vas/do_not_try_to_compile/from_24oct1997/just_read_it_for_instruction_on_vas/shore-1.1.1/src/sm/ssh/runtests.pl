#!/bin/sh -- # do not change this line, it mentions perl and prevents looping
eval 'exec perl -s $0 ${1+"$@"}'
	if 0;
#
#  $Header: /p/shore/shore_cvs/src/sm/ssh/runtests.pl,v 1.11 1997/05/23 20:55:00 nhall Exp $
#

$v=1;
$w=1;
$cpysrc = <<EOF;
/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
EOF

use FileHandle;

# Complete list of the tests:

# Set $kind to one of : crash abort yield delay

# NB: below this, the list gets reset if you
# want to run a subset of the tests.
# Also note: they are run in reverse order.

$kind = crash;

$tests_ = <<EOF;
    extent.3/1/alloc.2
    store.1/1/file.undo.1
    store.1/1/vol.2

    rtree.1/1/rtree.1
    rtree.2/1/rtree.1
    rtree.3/1/rtree.1
    rtree.4/1/rtree.1
    rtree.5/1/rtree.3

    enter.2pc.1/1/trans.1
    recover.2pc.1/1/trans.2
    recover.2pc.2/1/trans.2
    extern2pc.commit.1/1/trans.1

    prepare.readonly.1/1/trans.1
    prepare.readonly.2/1/trans.1
    prepare.abort.1/1/trans.1
    prepare.abort.2/1/trans.1

    prepare.unfinished.0/1/trans.1
    prepare.unfinished.1/1/trans.1
    prepare.unfinished.2/1/trans.1
    prepare.unfinished.3/1/trans.1
    prepare.unfinished.4/1/trans.1
    prepare.unfinished.5/1/trans.1

    commit.1/1/trans.1
    commit.2/1/trans.1
    commit.3/1/trans.1

    io_m::_mount.1/1/vol.init
    io_m::_dismount.1/1/vol.init

    commit.1/1/btree.6
    commit.2/1/btree.6
    commit.3/1/btree.6
    extent.3/1/btree.6
    io_m::_dismount.1/1/btree.6
    io_m::_mount.1/1/btree.6

    compensate/1/btree.1
    compensate/1/bt.remove.1

    commit.1/1/bt.remove.1
    commit.2/1/bt.remove.1
    commit.3/1/bt.remove.1
    extent.3/1/bt.remove.1
    btree.bulk.1/1/btree.3
    btree.bulk.2/1/btree.3
    btree.bulk.3/1/btree.3

    btree.remove.9/1/bt.remove.1
    btree.unlink.4/1/bt.remove.1
    btree.unlink.3/1/bt.remove.1
    btree.unlink.1/1/bt.remove.1
    btree.unlink.2/1/bt.remove.1

    btree.shrink.1/1/btree.5
    btree.shrink.2/1/btree.5
    btree.shrink.3/1/btree.5
    btree.create.1/1/btree.6
    btree.create.2/1/btree.6
    btree.distribute.1/1/btree.6
    btree.grow.1/1/btree.6
    btree.grow.2/1/btree.6
    btree.grow.3/1/btree.6
    btree.insert.1/1/btree.6
    btree.insert.2/1/btree.6
    btree.insert.3/1/btree.6
    btree.insert.4/1/btree.6
    btree.insert.5/1/btree.6

    btree.propagate.s.10/1/btree.6
    btree.propagate.s.1/1/btree.6
    btree.propagate.s.2/1/btree.6
    btree.propagate.s.3/1/btree.6
    btree.propagate.s.4/1/btree.6
    btree.propagate.s.5/1/btree.6
    btree.propagate.s.6/1/btree.6
    btree.propagate.s.7/1/btree.6
    btree.propagate.s.8/1/btree.6
    btree.propagate.s.9/1/btree.6

    btree.remove.6/1/btree.33

    btree.propagate.d.1/1/bt.remove.1
    btree.propagate.d.3/1/bt.remove.1
    btree.propagate.d.4/1/bt.remove.1
    btree.propagate.d.5/1/bt.remove.1
    btree.propagate.d.6/1/bt.remove.1
    btree.remove.1/1/bt.remove.1
    btree.remove.2/1/bt.remove.1
    btree.remove.3/1/bt.remove.1
    btree.remove.4/1/bt.remove.1
    btree.remove.5/1/bt.remove.1
    btree.remove.7/1/bt.remove.1
    btree.remove.10/1/bt.remove.1

    btree.1page.1/1/btree.convert.1
    btree.1page.2/1/btree.convert.1

EOF

# reset $tests_ to a subset:


@tests = split(/\s+/, $tests_);

NEXT: foreach $file (@ARGV) {
    # $file is test name
    
    $j=0;
    while($j++ < $#tests) {
	$t = @tests[$j];
	($tst,$val,$script) = split(/[\/]/, $t);

	# print STDOUT "$j($#tests): trying to match $tst, $s\n";

	$tst =~ m/^($file)$/o  && do {
	   $res = &onetest($tst,$val,$script);
	   next NEXT;
	};

    }
    print STDERR "Cannot find  $s in list.\n";
    exit 1;
}
if($i > 0) { exit 0; };

while($#tests>0) {
    $t = pop(@tests);

    ($tst,$val,$script) = split(/[\/]/, $t);

    $res = &onetest($tst,$val,$script);

}

sub onetest {
    my($test,$val,$script)=@_;

    print STDOUT "\n*** $test\n";
    $status = `/bin/rm -f log/* volumes/*`;
    if($status != 0) { return 1; }

    $system="./runtest $test $val $kind crashtest $script";
    print STDOUT "    $system\n";
    system $system;
    $status = $?;
    # print STDOUT "    STATUS $status\n";

    if($status == 2) {
       print STDOUT "    exiting STATUS $status\n";
       exit $status;
    }

    $status = $status/256;
    # print STDOUT "    STATUS $status\n";
    if($status != 44) {
       print STDOUT "    FAILED TO CRASH $test $val\n";
       return 2;
    }

    print STDOUT "    RECOVERY ... ";

    $ENV{"DEBUG_FLAGS"}="restart_m::redo_pass xct_t::rollback xct_impl.c log_m::insert";
    $ENV{"SSMTEST"}="";
    $ENV{"SSMTEST_ITER"}="";
    $ENV{"SSMTEST_KIND"}="";

    $system="./runtest \"\" "
	.  $val
	.  " $kind"
	.  " \"restart_m::redo_pass xct_t::rollback log_m::insert\" "
	.  " empty";

    # print STDOUT "    $system\n";
    system $system;
    $status = $?;

    print STDOUT "    STATUS $status\n";
    if($status >= 32768) {
       exit $status;
    }

    # print STDOUT "    STATUS $status\n";
    $status = $status/256;
    if($? != 0) {
       print STDOUT " FAILED \n";
       exit;
       return 3;
    }
    print STDOUT "    WORKED \n";
    return 0;
}
