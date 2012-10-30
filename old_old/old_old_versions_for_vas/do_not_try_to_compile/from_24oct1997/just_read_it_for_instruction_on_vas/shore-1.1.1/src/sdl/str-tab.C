/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#include "node.h"
#include "ShoreApp.h"
#include "sdl_internal.h"
#include "sdl_ext.h"
#include "sdl-gram.tab.h"
extern int lineno;

node *string_tab[STR_TABSIZE]; // hash table for string nodes

struct rpair reserve_words[] = {
	{"interface",	 INTERFACE},
	{"dcolon",	 DCOLON},
	{"const",	 CONST},
	{"rshift",	 RSHIFT},
	{"lshift",	 LSHIFT},
	{"TRUE",	 tTRUE},
	{"FALSE",	 tFALSE},
	{"typedef",	 TYPEDEF},
	{"float",	 FLOAT},
	{"double",	 DOUBLE},
	{"long",	 LONG},
	{"short",	 SHORT},
	{"int",		 LONG}, // int is an sdl addition; may be dubious.
	{"unsigned",	 UNSIGNED},
	{"char",	 CHAR},
	{"boolean",	 BOOLEAN},
	{"octet",	 OCTET},
	{"any",	 ANY},
	{"struct",	 STRUCT},
	{"union",	 UNION},
	{"switch",	 SWITCH},
	{"case",	 CASE},
	{"default",	 DEFAULT},
	{"enum",	 ENUM},
	{"sequence",	 SEQUENCE},
	{"string",	 tSTRING},
	{"text",	 TEXT},
	{"attribute",	 ATTRIBUTE},
	{"readonly",	 READONLY},
	{"exception",	 EXCEPTION},
	{"void",	 VOID},
	{"out",	 tOUT},
	{"in",	 tIN},
	{"inout",	 tINOUT},
	{"raises",	 RAISES},
	// {"context",	 CONTEXT}, odl had context construct, but no known
	// semantics and it's commented out in the grammar anyway.
	{"oneway",	 ONEWAY},
	{"module",	 MODULE},
	{"attribute",	 ATTRIBUTE},
	{"direct",	 DIRECT},
	{"relationship",	 RELATIONSHIP},
	{"unique",	 UNIQUE},
	{"ordered",	 ORDERED},
	/* sdl additions */
	{"ref",		tREF},
	{"lref",	tLREF},
	{"set",	tSET},
	{"bag",	tBAG},
	// {"list",	LIST},
	// list is not defined, so may as well make it a syntax error.
	{"mulitilist",	MULTILIST},
	{"index",tINDEX},
	{"export",	EXPORT},
	{"import",	IMPORT},
	{"use",	USE},
	{"all",	ALL},
	{"as",	AS},
	{"private",	tPRIVATE},
	{"protected",	PROTECTED},
	{"public",	PUBLIC},
	{"static",	STATIC},
	{"override",	OVERRIDE},
	{"indexable",	INDEXABLE},
	{"inverse",	INVERSE},
	{"external",EXTERNAL},
	{"class",CLASS},
	{"pool", POOL},
	{"Pool",POOL},
	{ 0,0}
};
char * reserve_print_tab[LAST_TOKEN];

node * module_list = 0;
int debug_scan = 0;

int strhash ( char *str2)
/* string hashing function ; division algorithm.
** uses right shifts of succesive char values to break up
** permutations.
*/
{
	register int hashcnt, hashindex;
	hashcnt = hashindex = 0;
	while ( *str2 )
		hashcnt += (*str2++) << hashindex++;
	if (hashcnt > 0)
		return(hashcnt % STR_TABSIZE);
	else 
		return(-hashcnt % STR_TABSIZE);
}
extern "C" 
node *
get_string_node (char *string)
/* check for  string in main symbol table; return its node if found.
 * else insert it and return the new node.
 * this is used for everything.
 */
{
	node *hpt; 	/* pointers into string table*/
	int hindex;			/* hashed index to tabl*/
	static dodump = 0;
	hindex = strhash( string );
	hpt = string_tab[hindex];
	while (hpt)
	{
		if (!strcmp((char *)hpt->info, string))
			return(hpt);
		if (!hpt->next)
			break;
		hpt = hpt->next;
	}
	// if we got here, the string wasn't found,so copy it
	// to someplace permanent and insert in hash tab.
	char * save_space = new char[strlen(string)+1];
	strcpy(save_space,string);

	node * new_node;
	new_node = new node(0,(node *)save_space,string_tab[hindex]);
	string_tab[hindex] = new_node;
	return new_node;
}

char *
get_string(char * lstr)
// copy a db string into the string space, and return a pointer
// to the string, not the node.
{
	char strbuf[1024];
	if (!lstr) return 0;
	strcpy((char *)strbuf,lstr);
	node *str_node = get_string_node(strbuf);
	return (char *) str_node->info;
}

	

extern REF(ExprNode) 
get_literal_expr(TypeTag code, char *stval);

extern "C"
void
set_yylval(char *str,TypeTag code)
// for various literals, copy str into string space and set code
// appropriately, also setting yylval.
{
	node *npt = get_string_node(str);
	switch(code) {
	case Sdl_long: 
	case Sdl_string: 
	case Sdl_char:
	case Sdl_float:
	case Sdl_boolean:
	case Sdl_void:
	case Sdl_double:
		yylval.exprpt.set( get_literal_expr(code,(char *)npt->info));
	// this needs wor
	break;
	default:
		yylval.nodept = npt;
		// npt->code = code;
	}
	npt->lineno = ::lineno;
}

extern "C"
short
check_id_token(char *str)
// look up the string; if code is set, return it, else set it to ID
// also set yylval.
{
	node * npt = get_string_node(str);
	yylval.nodept = npt;
	if (npt->code == 0)
	{
		npt->code = ID;
	}
	npt->lineno = ::lineno; // always set at last use.
	if (debug_scan)
	 	fprintf(stderr,"scanned %d:%s\n",npt->code,npt->info);
	if (npt->code==DEFAULT)
		set_yylval(str,Sdl_void);
	return npt->code;
}

void
insert_rwords()
// insert reserve works in the string table
{
	int i=0;
	node * rpt;
	for (i=0;reserve_words[i].str; i++)
	{
		rpt = get_string_node(reserve_words[i].str);
		rpt->code = reserve_words[i].tok_num;
		// rpt->tag	= get_tag(rpt->code);
		reserve_print_tab[rpt->code] = (char *) rpt->info;
	}
}

void
dump_str_tab()
// dump the contents of the string table
{
	int i;
	for (i=0;i<STR_TABSIZE;i++)
	{
		node *spt = string_tab[i];
		while (spt)
		{
			printf("tok num %3d string %s\n",spt->code,spt->info);
			spt = spt->next;
		}
	}
}

