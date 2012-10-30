%{
#include "ShoreApp.h"
#include "metatypes.sdl.h"
#include "sdl_internal.h"
#include "sdl_ext.h"
#include "sdl-gram.tab.h"
int lineno = 1;
extern int inc_lineno();
extern "C"
int 
check_linecount(char *);
extern "C"
short
check_id_token(char *);
extern "C"
int inc_linecount();

%}
%%
"<->"	{ return BI_ARROW; }
"::"	{ return DCOLON; }
"<<"	{ set_yylval(yytext,NO_Type); return LSHIFT; }
">>"	{ set_yylval(yytext,NO_Type); return RSHIFT; }
[a-zA-Z][a-zA-Z0-9_]* { 
	return check_id_token(yytext);
}
[0-9]+	{
	set_yylval(yytext, Sdl_long);
	return integer_literal; 
}
0x[0-9a-fA-F]+	{
	set_yylval(yytext, Sdl_long);
	return integer_literal; 
}
\"(([^\"])|"\\\"")*\" {
	set_yylval(yytext, Sdl_string);
	return string_literal; 
}
\'(([^\'])|"\\\'")*\' {
	set_yylval(yytext, Sdl_char);
	return character_literal; 
}
[0-9]+"."[0-9]*([eE]([+-])?[0-9]+)? {
	set_yylval(yytext, Sdl_double);
	return floating_pt_literal; 
}
[0-9]+("."[0-9]*)?([eE]([+-])?[0-9]+) {
	set_yylval(yytext, Sdl_double);
	return floating_pt_literal; 
}
[ \t]+ /* ignore whitespace */ 
[\n]	{ inc_linecount(); } /* count lines */
"#"[^\n]*[\n] { check_linecount(yytext); } /* cpp directive; check for line # */
"//"[^\n]* /* ignore // style comments */
.	{  /* pass through everything else */ 
	yylval.code = yytext[0];
	return yytext[0];
}
"#"[^\n]*
%%
