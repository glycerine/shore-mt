#!/bin/perl
#
#  $Header: /p/shore/shore_cvs/tools/stats.pl,v 1.14 1997/09/19 11:44:27 solomon Exp $
#
use Getopt::Std;

$opt_C = 0;
$opt_v = 0;
die "usage: $0 [-vC] file ...\n"
	unless getopts("vC");
# *************************************************************
#
# usage: <this-script> [-v] [-C] filename [filename]*
#
# -v verbose
# -C issue C code that can be compiled by a C compiler
# 
# *************************************************************
#
# INPUT: any number of sets of stats for software
#	layers called "name", with (unique) masks as follows:
#
#	name = mask class {
#	type STATNAME	Error string
#	type STATNAME	Error string
#	 ...
#	type STATNAME	Error string
#	}
#
#	"type" must be a one-word type:
#		int, u_int, float 	are the only types the 
#		long, u_long
#		statistics package knows about.

#	if "type" is any of the following types, it's translated:
#		u_int, u_long -- translated to "unsigned int", "unsigned long" etc
#		
#	"name" is used for module name; it's printed before printing the 
#		group of statistics for that module 
#
#  "mask" can be in octal (0777777), hex (0xabcdeff) or decimal 
#   	notation
#
#   "class" is required. Output files (.i) are prefixed by class name.
#
#	Error string will be quoted by the translator.  Don't
#		put it in quotes in the .dat file.
#
#	Stat_info[] structure is meant to be part of a class:
#		w_error_info_t <class>::stat_names[] = ...

# *************************************************************
#
# OUTPUT:
#  for each class  this script creates:
#	MSG:  <class>_msg.i		-- the strings
#	DEFF: <class>_def.i 	-- #defined manifest constants
#	STRUCT: <class>_struct.i -- the statistics variables
#	CODE: <class>_op.i 		-- the operator<< 
#
# *************************************************************
#
#

$cpysrc = <<EOF;
/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
EOF


# initialize:
# can't initialize with proper basename
# until we read the name of the package

$basename = "unknown";



foreach $FILE (@ARGV) {
	&translate($FILE);
	if($opt_v) { printf(STDERR "done\n");}
}

sub pifdef {
	local($F)=@_[0];
	if($opt_C) {
		printf($F "#ifdef __cplusplus\n");
	}
}
sub pelse {
	local($F)=@_[0];
	if($opt_C) {
		printf($F "#else /*__cplusplus*/\n");
	}
}
sub pendif {
	local($F)=@_[0];
	if($opt_C) {
		printf($F "#endif /*__cplusplus*/\n");
	}
}

sub head {
	local($F)=@_[0];
	local($fname)=@_[1];

	local($x) = $fname;
	$x =~ s/\.i/_i__/;
	$x =~ s/\.h/_h__/;

	if($opt_v) {
		printf(STDERR 
		"head: trying to open "."$fname\n");
	}
	open($F, ">$fname") || 
		die "cannot open $fname: $!\n";


	printf($F "#ifndef __$x\n");
	printf($F "#define __$x\n");
}
sub foot {
	local($F)=@_[0];
	local($fname)=@_[1];

	local($x) = $fname;
	$x =~ s/\.i/_i__/;
	$x =~ s/\.h/_h__/;

	printf($F "#endif /*__$x*/\n");
	if($opt_v) {
		printf(STDERR 
		"head: trying to close "."$fname\n");
	}
	close($F);
}

sub translate {
	local($file)=@_[0];
	local($warning)="";
	local($base)=\e;
	local($typelist)='"';

	open(FILE,$file) || die "Cannot open $file\n";
	if($opt_v) { printf (STDERR "translating $file ...\n"); }
	$warning = 
			"\n/* DO NOT EDIT --- GENERATED from $file by stats.pl */\n\n";

	LINE: while (<>)  {
		{
			# Handle comments -- some start with # after whitespace
			next LINE if (m/^\s*#/);
			# some are c++-style comments
			next LINE if (m=^\s*[/]{2}=);
		}

		# { to match the one in the next line (pattern)
		s/\s*[}]// && do {
			if($opt_v) { 
				printf(STDERR 
				"END OF PACKAGE: ".$basename.",".$BaseName." = 0x%x\n", $base);
			}

			{
				# final stuff before closing files...

				printf(DEFF "#define $BaseName%-20s 0x%x\n",'_STATMIN', $base);
				printf(DEFF "#define $BaseName%-20s 0x%x\n",'_STATMAX', $highest);
				printf(DEFF "\n");

				print MSG "\t\"dummy stat code\"\n";

				printf(CODE "w_rc_t e;\n");
				printf(CODE 
					"    if(e = s.add_module_static(\"$description\",0x%x,%d,".
					"t.stat_names,t.stat_types,(&t.$first)+1)){\n",
					$base,$cnt+1 );
				printf(CODE "        cerr <<  e << endl;\n");
				printf(CODE "    }\n");

				#{ to match theone in the next line
				printf(CODE "    return s;\n}\n");
				&pendif(CODE);

				&pifdef(CODE);
				printf(CODE "const\n");
				&pendif(CODE);
				printf(CODE "\t\tchar	*$class"."::stat_types =\n");
				printf(CODE "$typelist\";\n");

				&pifdef(STRUCT);
				printf(STRUCT "public: \nfriend w_statistics_t &\n");
				printf(STRUCT 
						"    operator<<(w_statistics_t &s,const $class &t);\n");
				printf(STRUCT "static const char	*stat_names[];\n");
				printf(STRUCT "static const char	*stat_types;\n");
				&pendif(STRUCT);
			}

			{
				&foot(DEFF,$DEFF_fname);
				&foot(MSG,$MSG_fname);
				&foot(CODE,$CODE_fname);
				&foot(STRUCT,$STRUCT_fname);
			}

			$description = "";
			$basename = "";
			$BaseName = "";
			$base = \e;
		};

		s/\s*(\S+)\s+([\s\S]*)[=]\s*([0xabcdef123456789]+)\s*(\S+)\s*[{]// && do
			# } to match the one in the pattern
		{ 
			# a new group
			$basename = $1;
			$description = $2;
			$base = $3;
			$class = $4;
	#		$first = "";
			$first = "_dummy_before_stats";
			$typelist='"'; # starting point

			$MSG_fname = $class."_msg.i";
			$DEFF_fname = $class."_def.i";
			$STRUCT_fname = $class."_struct.i";
			$CODE_fname = $class."_op.i";

			$BaseName = $basename;

			# translate lowercase to upper case 
			$BaseName =~ y/a-z/A-Z/;
			$base = oct($base) if $base =~ /^0/;
			if($class){
				if($opt_v) {
					printf(STDERR "CLASS=$class\n");
				}
			} else {
				printf(STDERR "missing class name.");
				exit 1;
			}

			$cnt = -1;
			$highest = 0;

			if($opt_v) {
				printf(STDERR "PACKAGE: $basename,$BaseName = 0x%x\n", $base);
			}

			{
				# open each file and write boilerplate
				{
					&head(DEFF,$DEFF_fname);
					&head(CODE,$CODE_fname);
					&head(STRUCT,$STRUCT_fname);
					&head(MSG,$MSG_fname);
				}

				# WARNINGS
				{
					# add warning and copyright to each file
					print DEFF $warning.$cpysrc;
					print MSG $warning.$cpysrc;
					print STRUCT $warning.$cpysrc;
					print CODE $warning.$cpysrc;
				}
				# end of boilerplate

				# stuff after boilerplate
				{
					&pifdef(CODE);
					printf(CODE "w_statistics_t &\n");
					printf(CODE 
						"operator<<(w_statistics_t &s,const $class &t)\n{\n");#}

					&pifdef(STRUCT);
					printf(STRUCT " w_stat_t $first;\n");
					&pelse(STRUCT);
					if($opt_C) {
						printf(STRUCT " int $first;\n");
					}
					&pendif(STRUCT);
				}

			} # end opening files and writing tops of files
		};  
		

		next LINE if $base =~ \e;
		# peel off whitespace at beginning
		s/^\s+//;

		($typ, $def, $msg) = split(/\s+/, $_, 3);
		next LINE unless $def;

		# peel semicolon off def if it's there
		$def =~ s/;$//;

#		{
#			#  save the name of the first value in the set
#			if($cnt==-1) {
#				$first = "$def";
#			}
	#	}
		{
			# update counters
			++$cnt;
			$val = $cnt + $base;
			if($highest < $val) { $highest = $val; }
		}
		{
			# take newline off msg
			chop $msg;
			# put the message in double quotes
			$msg = qq/$msg/;
		}

		{
			# clean up abbreviated types
			$typ =~ s/u_int/unsigned int/;
			$typ =~ s/u_long/unsigned long/;

			if ($typ =~ m/unsigned long/) {
				$typechar = "v";
			} elsif ($typ =~ m/([a-z])/) {
				$typechar = $1;
			}
			$typelist .= $typechar;
			if($opt_v) {
				printf(STDERR "typelist is $typelist\n");
			}
		}
		{
			# do the printing for this line

			# printf(DEFF "#define $BaseName"."_$def"."_relative     0x%x\n",$cnt);
			printf(DEFF "#define $BaseName"."_$def"."              0x%08x,$cnt\n",$base);

			printf(MSG "/* $BaseName%s%-18s */ \"%s\",\n",  '_', $def, $msg);

			printf(STRUCT " $typ $def;\n");

		}

	} # LINE: while

	if($opt_v) { printf(STDERR "translated $file\n");}

	close FILE;
}

