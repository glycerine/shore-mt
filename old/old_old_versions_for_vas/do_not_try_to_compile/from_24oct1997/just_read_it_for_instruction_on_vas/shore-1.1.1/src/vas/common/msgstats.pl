# --------------------------------------------------------------- #
# -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- #
# -- University of Wisconsin-Madison, subject to the terms     -- #
# -- and conditions given in the file COPYRIGHT.  All Rights   -- #
# -- Reserved.                                                 -- #
# --------------------------------------------------------------- #
use strict 'subs';
use Getopt::Std;

#
# use switches
# -n for nfs
# -m for mount
# -o for other
# -c for ??
# -r for ??
#
# -v for values
# -s for strings
#

$opt_n = $opt_m = $opt_o = $opt_v = $opt_s = $opt_r = $opt_c = 0;
die "usage: $0 -[sv] -[mncro]\n"
	unless getopts("nmovsrc");

die "Must choose one of: -s -v\n"
	unless $opt_v + $opt_s == 1;
die "Must choose one of: -m -n -c -r -o\n"
	unless $opt_n + $opt_m + $opt_o + $opt_r + $opt_c == 1;

if($opt_r || $opt_c) {
	$started = 0;
} else {
	$started = 1;
}

while(<>) {
	if($opt_n) {
		/^#define (NFSPROC\w+).*unsigned long.\s*(\d+)/
			&& doprint($2,$1);
		/^#define (NFSPROC\w+).*u_long.\s*(\d+)/
			&& doprint($2,$1);
	} 
	if($opt_m) {
		/^#define (MOUNTPROC\w+).*unsigned long.\s*(\d+)/
			&& doprint($2,$1);
		/^#define (MOUNTPROC\w+).*u_long.\s*(\d+)/
			&& doprint($2,$1);
	} 
	if($opt_c || $opt_r) {
		/^#define (\w+).*unsigned long.\s*(\d+)/
			&& doprint($2,$1);
		/^#define (\w+).*u_long.\s*(\d+)/
			&& doprint($2,$1);
	}
}
sub doprint {
	local ($val,$str) = @_;

	if($str =~ m/czero/) {
		if($opt_c) {
			$started = 1;
		}
		if($opt_r) {
			$started = 0;
		}
	} elsif($str =~ m/vzero/) {
		if($opt_r) {
			$started = 1;
		}
		if($opt_c) {
			$started = 0;
		}
	} elsif($started==1)  {
		if($opt_s) {
			# string
			print "/* ".$val." */ \"".$str."\",\n" ;
		} else {
			# value
			print "$val,\n";
		}
	}
}
