#!/bin/perl
#  $Header: /p/shore/shore_cvs/tools/shell.pl,v 1.7 1997/09/19 11:44:26 solomon Exp $

$cpysrc = <<EOF;
/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
EOF


open(H, ">shell.h") || die "cannot open shell.h: $!\n";
open(CMD, ">shell.commands") || die "cannot open shell.commands: $!\n";
open(ALIAS, ">shell.aliases") || die "cannot open shell.aliases: $!\n";
open(HELP, ">shell.help") || die "cannot open shell.help: $!\n";
open(HELPTXT, ">shell.help_txt") || die "cannot open shell.help: $!\n";
open(USAGE, ">shell.usage") || die "cannot open shell.usage: $!\n";
open(KEY, ">shell.keywords") || die "cannot open shell.usage: $!\n";
open(GENH, ">shell.generated.h") || die "cannot open shell.generated.h: $!\n";

$warning = "\n/* DO NOT EDIT --- GENERATED FROM shell.pl  */\n\n";
print H $warning;
print H $cpysrc;
print HELP $warning;
print HELP $cpysrc;
print CMD $warning;
print CMD $cpysrc;
print ALIAS $warning;
print ALIAS $cpysrc;
print HELPTXT $warning;
print HELPTXT $cpysrc;
print USAGE $warning;
print USAGE $cpysrc;
print KEY $warning;
print KEY $cpysrc;
print GENH $warning;
print GENH $cpysrc;



print GENH "extern char* _help_txt"."[];\n";
print HELPTXT "char* _help_txt"."[] = {\n";

print GENH "typedef struct help { int from; int to; } _help_struct;\n";
print GENH "extern _help_struct _help"."[];\n";
print HELP "_help_struct  _help"."[] = {\n";

print GENH 
	"#define REQUIRED_ARGS ClientData, Tcl_Interp* ip, int ac, char* av"."[]\n";

print GENH "typedef int (*_cmdfunc)(REQUIRED_ARGS);\n";
print GENH "typedef _cmdfunc cmdfunc;\n";

print H "#include \"shell.generated.h\"\n";

print GENH 
"typedef struct command {\n\tchar *name;\n\tcmdfunc func;\n\tint cmdnum;\n} _command_struct;\n";
print GENH "extern _command_struct  _commands"."[];\n";
print GENH "extern _command_struct  _aliases"."[];\n";
print GENH "extern char *_usage"."[];\n";

print CMD   "_command_struct  _commands"."[] = {\n";
print ALIAS "_command_struct  _aliases"."[] = {\n";
print USAGE "char *_usage"."[] =  {\n";

$ncmds = -1;
$help_cnt = 0;
$help_cnt_start = 0;
$line=0;
$in_mode=0;

LINE:
while (<>)  {
	chop;
	++$line;
	if($v) { printf(STDOUT "line $.: $_\n" ); }
    next LINE if (/^[!]/); 	# discard comments
	goto DOIT if (/^%%/);	# print info
	goto MODE if (/^%[^%]/);	# change modes
	next LINE unless (/\S/);

	printf(HELPTXT " /*%s,%d*/ ", $cmd, $help_cnt);
	printf(HELPTXT " \"%s\", \n", $_);
	++$help_cnt;

	next LINE;

	MODE: {
		# %COMMAND command
		if(/^%COMMAND/) {
			if($in_mode != 0) {
				printf(STDERR 
		"Did you forget a \"%%%%\" at the end of HELP for $cmd? (line $.)\n");
				printf(STDERR "$cmd skipped...\n");
			}
			$in_mode = 1;
			++$ncmds;
			$help_cnt_start = $help_cnt;
			($m, $cmd) = split(/\s+/, $_, 2);
			$commands{$ncmds} = $cmd;
			if($v) { printf(STDOUT "line $.: command # $ncmds = $cmd\n" ); }
			last MODE; 
		}
		if(/^%USAGE/) {	
			($m, $usage) = split(/\s+/, $_, 2);
			$usage[$ncmds]=$usage;
			if($v) { printf(STDOUT "line $.: usage $usage\n" ); }
			;last MODE; 
		}
		if(/^%ALIAS/) {
			($m, $remainder) = split(/\s+/, $_, 2);
			@aliases = split(/\s+/, $remainder);
			;last MODE; 
		}
		if(/^%KEY/) {
			($m, $remainder) = split(/\s+/, $_, 2);
			@k = split(/\s+/, $remainder);
			while($#k >=0) {
				$a = pop(@k);
				@keywords{$a} = join(';', @keywords{$a}, $ncmds);
			}
			;last MODE; 
		}
		if(/^%HELP/) {	
			($m, $remainder) = split(/\s+/, $_, 2);
			printf(HELPTXT " /*%s,%d*/ ", $cmd, $help_cnt);
			printf(HELPTXT " \"%s\", \n", $remainder);
			++$help_cnt;
			;last MODE; 
		}
	}
	next LINE;


	DOIT: {
			{
				printf(H "#define __cmd_%s__ %d \n", $cmd, $ncmds);
				printf(H "extern \"C\" int cmd_%s(REQUIRED_ARGS);\n", $cmd);

				printf(CMD " { \"%s\", cmd_%s, %d }, \n", 
					$cmd, $commands{$ncmds}, $ncmds);
				printf(USAGE " /*%s,%d*/  \"%s\", \n", $cmd, $ncmds, $usage);
				printf(HELP "{ /*%s,%d*/ %d,%d},\n", $cmd, $ncmds,
					$help_cnt_start, $help_cnt-1);
			}
			while($#aliases>=0) {
				$a = pop(@aliases);

				# GROT special case for the history comand (!!)
				$a =~ s/\\041/shriek/g;

				printf(ALIAS " { \"%s\", cmd_%s, %d }, \n", $a, 
					$commands{$ncmds}, $ncmds);
				printf(H "#define __cmd_%s__ %d \n", $a, $ncmds);

				## add a to the list of keywords
				## associated with this command
				@keywords{$a} = join(';', @keywords{$a}, $ncmds);
			}
			$in_mode = 0;
	}
	next LINE;
}
print CMD " { 0, 0, 0} \n}; /* _command */";
print ALIAS " { 0, 0, 0} \n}; /* _aliases */";
print USAGE "}; /* _usage */";
print HELPTXT "}; /* _help_txt */";
print HELP "}; /* _help */";

printf(H "#define __num_commands__ %d", ++$ncmds);

print GENH "typedef struct keywd {char *word; char *helpindex;} _keyword_struct;";
print GENH "extern _keyword_struct  _keywords"."[];\n";
print KEY "_keyword_struct  _keywords"."[]={\n";

REST: {
	@keys = keys keywords;
	@values = values keywords;
	while($#keys>=0) {
		$a = pop(@keys);
		printf(KEY "  { \"%s\", \"%s\" },\n", $a, pop(@values));
	}
}
printf(KEY "  { 0,0 }\n}; /* _keywords */");

close IN;
close H;
close HELP;
close HELPTXT;
close CMD;
close ALIAS;
close USAGE;
close KEY;
close GENH;

