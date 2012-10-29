#!/s/std/bin/perl -w

# <std-header style='perl' orig-src='shore'>
#
#  $Id: logdef.pl,v 1.17 2010/10/27 17:04:32 nhall Exp $
#
# SHORE -- Scalable Heterogeneous Object REpository
#
# Copyright (c) 1994-99 Computer Sciences Department, University of
#                       Wisconsin -- Madison
# All Rights Reserved.
#
# Permission to use, copy, modify and distribute this software and its
# documentation is hereby granted, provided that both the copyright
# notice and this permission notice appear in all copies of the
# software, derivative works or modified versions, and any portions
# thereof, and that both notices appear in supporting documentation.
#
# THE AUTHORS AND THE COMPUTER SCIENCES DEPARTMENT OF THE UNIVERSITY
# OF WISCONSIN - MADISON ALLOW FREE USE OF THIS SOFTWARE IN ITS
# "AS IS" CONDITION, AND THEY DISCLAIM ANY LIABILITY OF ANY KIND
# FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
#
# This software was developed with support by the Advanced Research
# Project Agency, ARPA order number 018 (formerly 8230), monitored by
# the U.S. Army Research Laboratory under contract DAAB07-91-C-Q518.
# Further funding for this work was provided by DARPA through
# Rome Research Laboratory Contract No. F30602-97-2-0247.
#
#   -- do not edit anything above this line --   </std-header>

#
#  Perl script for generating log record types.
#

use strict;
use Getopt::Long;


sub Usage
{
    my $progname = $0;
    $progname =~ s/.*[\\\/]//;
    print STDERR <<EOF;
Usage: $progname [--help] logdef_file
Generate C++ which represents log records described by input file. 

    --help|h     print this message and exit.
EOF
}

my %options = (help => 0);
my @options = "help|h!";
my $ok = GetOptions(\%options, @options);
$ok = 0 if $#options != 0;
if (!$ok || $options{help})  {
    Usage();
    die(!$ok);
}

my $infile = $ARGV[0];

open INPUT, "<$infile"		or die "cannot open $infile\n";

open FUNC,  ">logfunc_gen.h"	or die "cannot open logfunc_gen.h\n";
open TYPE,  ">logtype_gen.h"	or die "cannot open logtype_gen.h\n";
open DEF,   ">logdef_gen.cpp"	or die "cannot open logdef_gen.cpp\n";
open STUB,  ">logstub_gen.cpp"	or die "cannot open logstub_gen.cpp\n";
open FUDGE,  ">logfudge_gen.cpp"	or die "cannot open logfudge_gen.cpp\n";
open REDO,  ">redo_gen.cpp"	or die "cannot open redo_gen.cpp\n";
open UNDO,  ">undo_gen.cpp"	or die "cannot open undo_gen.cpp\n";
open STR,   ">logstr_gen.cpp"	or die "cannot open logstr_gen.cpp\n";

my $timeStamp = localtime;
my $header = <<EOF;
/* DO NOT MODIFY --- generated by $0 from $infile 
                     on $timeStamp

<std-header orig-src='shore' genfile='true'>

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

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/


EOF

print FUNC "#ifndef LOGFUNC_GEN_H\n#define LOGFUNC_GEN_H\n\n";

print FUNC $header;
print TYPE $header;
print DEF $header;
print STUB $header;
print FUDGE $header;
print FUDGE "\n\n double logfudge_factors[] = {\n";
print REDO $header;
print UNDO $header;
print STR $header;

printf(TYPE "enum kind_t { \n");

#
# create logtype.h
#

my $unique = 0;
LINE:
while (<INPUT>) {
    s/^\s*//;
    next LINE if (/^\#/ || ! /\w/);
    chop;
    while (! /;\s*$/) {
	$_ .= <INPUT>;
	chop;
    }
    s/\s+/ /g;
    my ($type, $attr, $fudge, $arg) = split(/[ \t\n]+/, $_, 4);
    chop $arg;

    my ($xflag, $sync, $redo, $undo, $format, $aflag, $logical) = split(//, $attr);
    my $cat = &get_cat($redo, $undo, $format, $logical);
    
    printf(TYPE "\tt_$type = %d,\n", $unique++);
    &def_rec($type, $xflag, $aflag, $sync, $redo, $undo, $cat, $fudge, $arg);
				# 
    print REDO "\tcase t_$type : \n";
    if ($redo) {
	printf(REDO "\t\t((%s_log *) this)->redo(page); \n", $type);
    } else {
	print REDO "\t\tW_FATAL(eINTERNAL);\n";
	}
    print REDO "\t\tbreak;\n";
    
    
    print UNDO "\tcase t_$type : \n";
    if ($undo) {
	printf(UNDO "\t\t((%s_log *) this)->undo(page); \n", $type);
    } else {
	print UNDO "\t\tW_FATAL(eINTERNAL);\n";
    }
    print UNDO "\t\tbreak;\n";

    printf(STR "\tcase t_%s : \n\t\treturn \"%s\";\n", $type, $type);
}
printf(TYPE "\tt_max_logrec = %d\n", $unique);
print TYPE "};\n";

print FUNC "\n\n#endif\n";
print FUDGE "\n\n 0.0 }; \n";

exit(0);

sub get_cat {
    my ($redo, $undo, $format, $logical) = @_;
    my ($ret);
    $ret = "0";
    $ret .= "|t_redo" if $redo;
    $ret .= "|t_undo" if $undo;
    $ret .= "|t_format" if $format;
    $ret .= "|t_logical" if $logical;
    if ($ret eq "0") { $ret = "t_status";};
    $ret;
}

sub def_rec {
    my ($type, $xflag, $aflag, $sync, $redo, $undo, $cat, $fudge, $arg) = @_;
    my ($class) = $type . "_log";
    my ($has_idx);

    my $redo_stmt = ($redo) ? 'void redo(page_p*);' : '';
    my $undo_stmt = ($undo) ? 'void undo(page_p*);' : '';
    print DEF<<CLASSDEF;
    class $class : public logrec_t {
	void fill(const lpid_t* p, uint2_t tag, int l) {
	  _cat = $cat, _type = t_$type;
	  logrec_t::fill(p, tag, l);
	}
      public:
	$class $arg;
	$class (logrec_t*)   {};
	
	$redo_stmt
	$undo_stmt
    };

CLASSDEF
	;

    #($real = $arg) =~ s/[\(\)]|const //g;
    $arg =~ s/\((.*)\)/$1/;
    # see if arg contains "int idx"
    $arg =~ /int\s+idx/ && do { $has_idx=1; };
    my $real = join(', ', grep(s/^.*\s+(\w+)$/$1/, split(/, /, $arg)));
	# print "arg = $arg\n";
	# print "real = $real\n";
    print "$type: cat = $cat\n";

    if ($xflag)  {
        my $func = "rc_t log_$type($arg)";

        print STUB $func, "\n";
        print STUB "{\n";
	my $page;
	($page = $real) =~ s/(\w*).*/$1/;
    	# print "page = $page\n";
	print STUB "    xct_t* xd = xct();\n";
        print STUB "    bool should_log = smlevel_1::log && smlevel_0::logging_enabled";
	if($aflag==0) {
	    if ($page eq "page") {
		print STUB "\n\t\t\t&& (page.get_store_flags() & page.st_tmp) == 0";
	    }
	    print STUB "\n\t\t\t&& xd && xd->is_log_on()";
	}
	print STUB ";\n";
	print STUB "    if (should_log)  {\n";
	print STUB "        logrec_t* logrec;\n";
	print FUDGE " $fudge, \n";
	if ($page eq "page") {
	    print STUB " // fudge $fudge \n";
	    print STUB "        W_DO(xd->get_logbuf(logrec, t_$type, &page));\n";
	} else {
	    print STUB " // fudge $fudge \n";
	    print STUB "        W_DO(xd->get_logbuf(logrec, t_$type));\n";
	}
        print STUB "        new (logrec) $class($real);\n";	   
	if ($page eq "page") {
	    print STUB "        W_DO(xd->give_logbuf(logrec, &page));\n";
	} else {
	    print STUB "        W_DO(xd->give_logbuf(logrec));\n";
	}
	if ($sync) {
	    print STUB "        W_COERCE( smlevel_0::log->flush_all() );\n";
	}
	print STUB "    }\n";
	if ($page eq "page") {
	    # no longer need to call set_dirty explicitly after give_logbuf 
	    print STUB "    else page.set_dirty();\n" if ($real);
	}
	print STUB "    return RCOK;\n";
    print STUB "}\n";

    print FUNC "extern \"C\" $func;\n";
    } 
}
