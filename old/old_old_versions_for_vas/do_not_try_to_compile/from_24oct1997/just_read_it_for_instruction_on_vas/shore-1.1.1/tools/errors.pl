#!/bin/perl -w
use strict 'subs';
use Getopt::Std;

$opt_e = 0;
$opt_c = 0;
$opt_d = 0;
$opt_v = 0;
die "usage: $0 [-vde]\n"
	unless getopts("vcde");
#
#  $Header: /p/shore/shore_cvs/tools/errors.pl,v 1.17 1997/09/19 11:44:25 solomon Exp $
#
# *************************************************************
#
# usage: <this-script> [-e] [-d] [-v] filename [filename]*
#
# -v verbose
# -c count occurrences of each error code in the sources (*.[chi])
#    turns off -e, -d.  Use as follows:
#	perl -s $(TOP)/tools/errors.pl -c -e <xxx.dat>
#    If you have the xxx*.i files intact, you'll see at least 1 count
#	for each error code; if not, you'll so 0 for those unused.
# -e generate enums
# -d generate #defines 
# 
# (both -e and -d can be used)
#
# *************************************************************
#
# INPUT: any number of sets of error codes for software
#	layers called "name", with (unique) masks as follows:
#
#	name = mask {
#	ERRNAME	Error string
#	ERRNAME	Error string
#	 ...
#	ERRNAME	Error string
#	}
#
#  (mask can be in octal (0777777), hex (0xabcdeff) or decimal 
#   notation)
#
#	(Error string can be Unix:ECONSTANT, in which case the string
#	for Unix's ECONSTANT is used)
#
#	If you want the error_info[] structure to be part of a class,
#	put the class name after the mask and groupname and before the open "{"
#	for the group of error messages; in that case the <name>_einfo.i
#	file will look like:
#		w_error_info_t <class>::error_info[] = ...
#	If you don't do that, the name of the error_info structure will
#	have the <name> prepended, e.g.:
#		w_error_info_t <name>_error_info[] = ...
#
# *************************************************************
#
# OUTPUT:
#  For each software layer (<name>), this script creates up to 5 files.
#  Examples are taken from <name> == "svas":
#      svas = 0x00100000 "Shore VAS" {
#            NotImplemented        Feature is not implemented
#            ...
#         }
#  In all cases the following two files are produced:
#    STR:  <name>_error.i
#      The error messages associated with codes, as a static array of strings
#         static char* svas_errmsg[] = {
#            /* SVAS_NotImplemented     */ "Feature is not implemented",
#            ...
#         };
#         const svas_msg_size = <number of entries>;
#    INFO: <name>_einfo.i  
#      An array of (code, message) pairs as an array of w_error_info_t
#         w_error_info_t svas_error_info[] = {
#             { SVAS_NotImplemented    , "Feature is not implemented" },
#            ...
#         };
#  If -d is used, two additional files are produced.
#    BINFO: <name>_einfo_bakw.i
#      Similar to <name>_einfo.i, but with string versions of the error
#      codes rather than the error messages
#         w_error_info_t svas_error_info_bakw[] = {
#              { SVAS_NotImplemented, "SVAS_NotImplemented" },
#            ...
#         };
#    HDRD: <name>_error_def.h
#      The #defined constants for the error codes, and for minimum and maximum
#      error codes
#         #define SVAS_OK                   0
#         #define SVAS_NotImplemented       0x100000
#         ...
#         #define SVAS_ERRMIN                0x100000
#         #define SVAS_ERRMAX                0x10003f
#  If -e is used, one additional file is produced.
#    HDRE: <name>_error.h
#      An enumeration for the codes, and an enum containing eERRMIN & eERRMAX
#         enum { 
#             svasNotImplemented       = 0x100000,
#             ...
#         };
#         enum {
#             svasERRMIN = 0x100000,
#             svasERRMAX = 0x10003f
#         };
# *************************************************************

if(!$opt_d && !$opt_e) {
	die "You must specify one of -d, -e.";
}
$both = 0;
if($opt_d && $opt_e) {
	$both = 1;
}

if($opt_c) {
	$both = 0;
	$opt_d = 0;
	$opt_e = 0;
}

$cpysrc = <<EOF;
/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
EOF


open(ERRS, "gcc -dM -E /usr/include/sys/errno.h |")
	|| die "cannot open /usr/include/sys/errno.h: $!\n";


# set up Unix error codes 
while (<ERRS>)  {
	next unless ($name,$value) = /^\#define \s+ (E\S*) \s+ (\S+)/x;
	$value = $errs{$value} if defined $errs{$value};
	$errs{$name} = $value;
}

$_error = '_error';
$_error_def = '_error_def';
$_einfo = '_einfo';
$_info = '_info';
$_errmsg = '_errmsg';
$_msg_size = '_msg_size';
$_OK	= 'OK';

foreach $FILE (@ARGV) {

	&translate($FILE);
	if($opt_v) { printf(STDERR "done\n");}
}

sub translate {
	local($file)=@_;
	local($warning)="";
	local($base)="\e";

	open(FILE,$file) || die "Cannot open $file\n";
	if($opt_v) { printf (STDERR "translating $file ...\n"); }
	$warning = 
			"\n/* DO NOT EDIT --- GENERATED from $file by errors.pl */\n\n";

	LINE: while (<>)  {
		next LINE if (/^#/);
		# {
		s/\s*[}]// && do {
			if($opt_v) { 
				printf(STDERR "END OF PACKAGE: ".$basename.",".$BaseName." = 0x%x\n", $base);
			}

			# {
			print INFO "};\n\n";
			if($class) {
				print(INFO 'void '.$class.'::init_errorcodes(){'."\n");
				print(INFO "\tif (! (w_error_t::insert(\n\t\t");
				print(INFO $groupname.', '.$class.'::error_info,',"\n\t\t");
				print(INFO $basename.'ERRMAX - '.$basename.'ERRMIN + 1)) ) {');
				print(INFO "\n\t\t\t W_FATAL(fcINTERNAL);\n\t}\n}\n");
			}
			if($opt_d) {
				# {
				print BINFO "};\n\n";
			}

			if($opt_e) {
				#{
				printf(HDRE "};\n\n");
				printf(HDRE "enum {\n");
				printf(HDRE "    %s%s = 0x%x,\n", $basename, 'ERRMIN', $base);
				printf(HDRE "    %s%s = 0x%x\n", $basename, 'ERRMAX', $highest);
				printf(HDRE "};\n\n");
			}
			if($opt_d) {
				printf(HDRD "#define $BaseName%s%-20s  0x%x\n", '_','ERRMIN', $base);
				printf(HDRD "#define $BaseName%s%-20s  0x%x\n", '_','ERRMAX', $highest);
				printf(HDRD "\n");
			}

			# {
			print STR "\t\"dummy error code\"\n};\n";
			print STR "const ".$basename.$_msg_size." = $cnt;\n\n";

			if($opt_e) {
				printf(HDRE "#endif /*__".$basename."_error_h__*/\n");
				close HDRE;
			}
			if($opt_d) {
				printf(HDRD "#endif /*__".$basename."_error_def_h__*/\n");
				close HDRD;
			}
			printf(STR "#endif /*__".$basename."_error_i__*/\n");
			close STR;
			printf(INFO "#endif /*__".$basename."_einfo_i__*/\n");
			close INFO;
			if($opt_d) {
				printf(BINFO "#endif /*__".$basename."_einfo_bakw_i__*/\n");
				close BINFO;
			}

			$basename = "";
			$BaseName = "";
			$base = "\e";
		};

		s/\s*(\S+)\s*[=]\s*([0xabcdef123456789]+)\s*(["].*["])\s*(.*)[{]// && do {
			$basename = $1;
			$base = $2;
			$groupname = $3;
			$class = $4;

			$BaseName = $basename;
			$BaseName =~ y/a-z/A-Z/;
			$base = oct($base) if $base =~ /^0/;
			if($class){
				if($opt_v) {
					printf(STDERR "CLASS=$class\n");
				}
			}

			$cnt = -1;
			$highest = 0;

			if($opt_v) { 
				printf(STDERR "PACKAGE: $basename,$BaseName = 0x%x\n", $base);
			}


			if($opt_d) {
				if($opt_v) {printf(STDERR "trying to open ".$basename.$_error_def.".h\n");}
				open(HDRD, ">$basename$_error_def.h") || 
					die "cannot open $basename$_error_def.h: $!\n";

				printf(HDRD "#ifndef __$basename%serror_def_h__\n", '_');
				printf(HDRD "#define __$basename%serror_def_h__\n", '_');
				printf(HDRD $cpysrc);
			}
			if($opt_d) {
				if($opt_v) {printf(STDERR "trying to open ".$basename."_einfo_bakw.h\n");}
				open(BINFO, ">$basename"."_einfo_bakw.i") || 
					die "cannot open $basename"."_einfo_bakw.i: $!\n";

				printf(BINFO "#ifndef __$basename"."_einfo_bakw_i__\n");
				printf(BINFO "#define __$basename"."_einfo_bakw_i__\n");
				printf(BINFO $cpysrc);
			}
			if($opt_e) {
				if($opt_v) {printf(STDERR "trying to open $basename$_error.h\n");}
				open(HDRE, ">$basename$_error.h") || 
					die "cannot open $basename$_error.h: $!\n";
				printf(HDRE "#ifndef __$basename%serror_h__\n", '_');
				printf(HDRE "#define __$basename%serror_h__\n", '_');
				printf(HDRE $cpysrc);
			}

			if($opt_v) {printf(STDERR "trying to open $basename$_error.i\n");}
			open(STR, ">$basename$_error.i") || 
				die "cannot open $basename$_error.i: $!\n";

			printf(STR "#ifndef __".$basename."_error_i__\n");
			printf(STR "#define __".$basename."_error_i__\n");
			printf(STR $cpysrc);

			if($opt_v) {printf(STDERR "trying to open $basename$_einfo.i\n");}
			open(INFO, ">$basename$_einfo.i") || 
				die "cannot open $basename$_einfo.i: $!\n";
			printf(INFO "#ifndef __".$basename."_einfo_i__\n");
			printf(INFO "#define __".$basename."_einfo_i__\n");
			printf(INFO $cpysrc);

			if($opt_e) {
				print HDRE $warning;
			} 
			if($opt_d) {
				print HDRD $warning;
				print BINFO $warning;
			}
			print STR $warning;
			print INFO $warning;

			if($class) {
				print INFO 
					"w_error_info_t ".$class."::error".$_info."[] = {\n"; #}
			} else {
				print INFO 
					"w_error_info_t ".$basename.$_error.$_info."[] = {\n"; #}
			}
			if($opt_e) { printf(HDRE "enum { \n"); } #}
			if($opt_d) { printf(HDRD "#define $BaseName%s%-20s 0\n", '_','OK');}
			printf(STR "static char* ".$basename.$_errmsg."[] = {\n"); #}

			if($opt_d) {
				print BINFO  
					"w_error_info_t ".$basename.$_error.$_info."_bakw[] = {\n"; #}
			}
		};  # } to match the one in the pattern
		

		next LINE if $base =~ "\e";
		($def, $msg) = split(/\s+/, $_, 2);
		next LINE unless $def;
		chop $msg;
		++$cnt;
		if($msg =~ m/^Unix:(.*)/) {
			# 		$def is unchanged
			$val = $errs{$1};
			$!= $val;
			$msg = qq/$!/; # force it to be a string
			$val = $val + $base;
		} else {
			$msg = qq/$msg/;
			$val = $cnt + $base;
		}
		if($highest < $val) { $highest = $val; }

		if($opt_c) {
			$msg = `fgrep $def *.[chi] | wc -l`;
			printf(STDOUT "$def: $msg");
		}
		if($opt_d) {
			printf(HDRD "#define $BaseName%s%-20s 0x%x\n", '_', $def, $val);
		}
		if($opt_e) {
			printf(HDRE "    $basename%-20s = 0x%x,\n", $def, $val);
		}

		if($opt_e) {
			printf(STR "/* $basename%-18s */ \"%s\",\n",  $def, $msg);
		} else {
			printf(STR "/* $BaseName%s%-18s */ \"%s\",\n",  '_', $def, $msg);
		}

		if($opt_e) {
			printf(INFO " { $basename%-18s, \"%s\" },\n", $def, $msg);
		} else {
			printf(INFO " { $BaseName%s%-18s, \"%s\" },\n", '_', $def, $msg);
		}

		if($opt_d) {
			printf(BINFO " { $BaseName%s%s, \"$BaseName%s%s\" },\n",
				'_', $def, '_', $def);
		}
	} # LINE: while

	if($opt_v) { printf(STDERR "translated $file\n");}

	close FILE;
}

