/****************** LEX **********************************************/

%{
#include <pFlex.h>
#include <defns.h>
#include <Bparsestate.h>
#include "metatypes.sdl.h"
#include <sdl_ext.h>
#include <oql_gram.tab.h>
%}

/** Other than keywords...	**/
O2ID		[a-zA-Z][a-zA-Z0-9_]*
O2CHAR		\'(([^\'])|"\\\'")*\'
O2STRING	\"(([^\"])|"\\\"")*\"
O2FLOAT_1	[0-9]+"."[0-9]*([eE]([+-])?[0-9]+)?
O2FLOAT_2	[0-9]+("."[0-9]*)?([eE]([+-])?[0-9]+)
O2INT_1		[0-9]+
O2INT_2		0x[0-9a-fA-F]+
O2COMMENTS_1  	\/\*(([^\*]*\*[^\/])*|([^\*]*))*\*\/
O2COMMENTS_2	\\\*.*$

/*******************************************************/
/** First the keywords	      	       	     	      **/
/*******************************************************/
%%
[ ]   	       {}
[\n\r] 	       {Bparser_state()->incLine();}
[\t]	       {}	
      /** Ignore tabs, whitespaces, returns, newlines	**/

   /** ODL keywords first...  **/

"interface"	{return INTERFACE;}
"module"	{return MODULE;}
"typedef"	{return TYPEDEF;}
"struct"	{return STRUCT;}
"union"		{return UNION;}
"const"		{return CONST;}
"enum"		{return ENUM;}
"except"	{return EXCEPT;}

"import"	{return IMPORT;}
"export"	{return EXPORT;}
"use"		{return USE;}
"as"		{return AS;}
"all"		{return ALL;}

"persistent"	{return PERSISTENT;}
"transient"	{return TRANSIENT;}
"public"	{return PUBLIC;}
"private"	{return tPRIVATE;}
"protected"	{return PROTECTED;}
"key"		{return KEY;}
"keys"		{return KEY;}
"extent"	{return EXTENT;}
"attribute"	{return ATTRIBUTE;}
"readonly"	{return READONLY;}
"relationship"	{return RELATIONSHIP;}
"inverse"	{return INVERSE;}
"order_by"	{return ORDER_BY;}
"override"	{return OVERRIDE;}
"in"		{return tIN;}
"out"		{return tOUT;}
"inout"		{return tINOUT;}
"raises"	{return RAISES;}
"direct"	{return DIRECT;}
"oneway"	{return ONEWAY;}
"context"	{return CONTEXT;}

"case"		{return CASE;}
"default"	{return DEFAULT;}
"switch"	{return SWITCH;}

"array"		{return ARRAY;}
"sequence"	{return SEQUENCE;}
"ref"		{return tREF;}
"lref"		{return tLREF;}
"bag"		{return tBAG;}
"set"		{return tSET;}
"list"		{return LIST;}
"multilist"	{return MULTILIST;}
"string"	{return tSTRING;}

";"   	       {return ';';}
":"   	       {return ':';}
"::"  	       {return DCOLON;}
","   	       {return ',';}
\(    	       {return '(';}
\)    	       {return ')';}
\[    	       {return '[';}
\]    	       {return ']';}
\{    	       {return '{';}
\}    	       {return '}';}
"<"   	       {return '<';}
">"   	       {return '>';}

\+    	       {return '+';}
\-    	       {return '-';}
\*    	       {return '*';}
\/    	       {return '/';}
"="   	       {return '=';}
\&    	       {return '&';}
\%    	       {return '%';}
\|    	       {return '|';}
\~    	       {return '~';}
"<<"  	       {return LSHIFT;}
">>"  	       {return RSHIFT;}
\^    	       {return '^';}

"+="	       {return PLUS_EQUAL;}
"-="	       {return MINUS_EQUAL;}

"any" 	       {return ANY;}
"octet"	       {return OCTET;}
"char"	       {return CHAR;}
"long"	       {return LONG;}
"short"	       {return SHORT;}
"double"       {return DOUBLE;}
"float"	       {return FLOAT;}
"unsigned"     {return UNSIGNED;}
"boolean"      {return BOOLEAN;}
"void"	       {return VOID;}

":="		{return(ASSIGN);}	/* BOLO extension */
"."		{return(DOT);}
"!="		{return(DIFF);}
"<>"		{return(DIFF);}
"=="		{return('=');}
"<="		{return(INFEQUAL);}
">="		{return(SUPEQUAL);}
\-">"		{return(ARROW);}
"define" 	{return(DEFINE);}
"count"		{return(COUNT);}
"sum"		{return(SUM);}
"min"		{return(OQL_MIN);}
"max"		{return(OQL_MAX);}
"avg"		{return(AVG);}
"element"	{return(ELT);}
"first"		{return(FIRST);}
"last"		{return(LAST);}
"distinct"	{return(DISTINCT);}
"unique"	{return(UNIQUE);}
"listoset"	{return(LISTOSET);}
"listtoset"	{return(LISTOSET);}
"flatten"	{return(FLATTEN);}
"abs"		{return(OQL_ABS);}
"not"		{return(NOT);}
\!		{return(NOT);}	
"like"		{return(LIKE);}
"intersect"	{return(INTERSECT);}
"mod"		{return('%');}
"and"		{return(AND);}
\&\&		{return(AND);}
"or"		{return(OR);}
\|\|		{return(OR);}
"unique set"	{return(USET);}
"tuple"		{return(TUPLE);}
"exists"	{return(EXISTS);}
"forall"	{return(FORALL);}
"for"		{return(FOR);}
".."		{return(DOTS);}
"select"	{return(SELECT);}
"from"		{return(FROM);}
"where"		{return(WHERE);}
"sort"		{return(SORT);}
"group"		{return(GROUP);}
"by"		{return(BY);}
"with"	        {return(WITH);}

"create"        {return(CREATE);}
"drop"          {return(DROP);}
"open"          {return(OPEN);}
"close"         {return(CLOSE);}
"db"            {return(DB);}
"index"         {return(tINDEX);}
"clustered"     {return(CLUSTERED);}
"on"            {return(OQL_ON);}

"insert"        {return INSERT;}
"into"          {return INTO;}
"delete"        {return DELETE;}
"destroy"       {return DESTROY;}
"update"        {return UPDATE;}
"apply"	        {return APPLY;}
"to"	        {return TO;}
"using"	        {return USING;}
"only"	        {return ONLY;}

"nil"           {return LEX_OQL_NIL;}
"false"	        {return LEX_FALSE;}
"true"	        {return LEX_TRUE;}
{O2ID}	        {return LEX_ID;}
{O2FLOAT_1}     {return LEX_FLOAT_LIT;}
{O2FLOAT_2}     {return LEX_FLOAT_LIT;}
{O2INT_1}       {return LEX_INT_LIT;}
{O2INT_2}       {return LEX_INT_LIT;}
{O2CHAR}        {return LEX_CHAR_LIT;}
{O2STRING}      {return LEX_STRING_LIT;}

{O2COMMENTS_1} {}
{O2COMMENTS_2} {}
.     	       {return ILLEGAL_INPUT;}

%%
/* <<EOF>>	       {return EOI;}  this seems to hose stuff. */
extern "C"
{
int yylex(YYSTYPE* yval)
{
   int x = Bparser_state()->scanner()->yylex();
   yval->string = (char *)(Bparser_state()->scanner()->YYText());
   return x;
}
}
