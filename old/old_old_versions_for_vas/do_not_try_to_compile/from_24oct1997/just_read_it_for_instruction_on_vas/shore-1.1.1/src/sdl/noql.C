/* --------------------------------------------------------------- */
/* -- Copyright (c) 1996 Computer Sciences Department,          -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#include <stdlib.h>
#include <stdio.h>
#include <ostream.h>
#include <stdlib.h>

#include <symbol.h>
#include <types.h>
#include <typedb.h>
#include <tree.h>

// Murali.add 1/6/95
#include <Aglob_vars.h>

#include <oql_context.h>

void check_defines(oqlContext &context)
{
	SymbolTable_i it(context.defines);
   Symbol	*sym;
   Type	*result;
   aqua_t	*define;
   Ref<sdlExprNode> shore_expr;


	for (; sym = *it; ++it)
   {
#if 0
      // cout << sym->name() << " flags: " << sym->flags() << '\n';
#endif
      if (sym->flags()) continue;
      define = (aqua_t *) sym->data();
      result = define->typecheck(context);
      cout << "Define " << sym->name() << " : " << *result << '\n';
      cout << *define << '\n';
      sym->setFlags(1);	/* don't typecheck again */
	  shore_expr = define->toShoreExpr();
	  // need to save this in the db somehow??
   }
}

int	preprint = 0;
int	convert = 1;
int	errors = 0;
int	typecheck = 1;
char	*_progname = 0;
int	prompting;
int	hoofer=1;
int	print_types = 0;         // I dont really care for the types...

int ProcessQuery(Ql_tree_node* root, oqlContext &);
int ProcessUtility(Ql_tree_node* root, oqlContext &);
//int ProcessUpdate(Ql_tree_node* root, oqlContext &);

    // Precondition: root != 0
int OqlExecuteQuery(Ql_tree_node* root, oqlContext& context)
{
   Type* query_type;
   aqua_t* aqua_tree;
   int oql_mode = 1;

   switch(root->_type)
   {
	case define_n:
      return ProcessQuery(root, context);
    case query_n: 
      return ProcessQuery(root->_kids[0], context);
    case utility_n:
      return ProcessUtility(root->_kids[0], context); 
/** Murali.change 3/6/95
  Updates also now go thru aqua_trees
    except the insert_values statement
**/
    case update_n:
      //if (root->_kids[0]->type() == iiv_n)
	 //return ProcessUpdate(root->_kids[0], context);
      //else
	 return ProcessQuery(root->_kids[0], context);
   }
   return -1;
}

extern FILE *bf;
void do_shore_q(Ref<sdlExprNode> );
	
extern int print_aqua;
extern int print_oql;
extern int print_oql2;

int ProcessQuery(Ql_tree_node* root, oqlContext& context)
{	 
   Type* query_type;
   aqua_t* aqua_tree;
   Ref<sdlExprNode> shore_expr;
   Ref<sdlExprNode> shore_ast;
   int oql_mode = 1;
	bf = stdout;

   // a temporary hack to fix parse problems.  This needs work...
   switch(root->type())
   {
	case function_or_create_n:
		root = new Ql_tree_node(cfw_n, 1,root);
	break;
	}

   if (print_oql2)
   {
	   shore_ast = root->shore_convert(context);
		shore_ast->print_sdl();
	}
   aqua_tree = root->aqua_convert(context);
   check_defines(context);
   if (typecheck) 
   {
      query_type = aqua_tree->typecheck(context);
      cout << "Result type: " << *query_type << '\n';
   }


   // Check if no error...
   if (query_type->me() == ODL_Error)
   {
      errstream() << "Typechecking failed. Tough luck" << endl;
      return paraQ_TYPECHK_ERROR;
   }

	if (print_aqua)
		aqua_tree->print(cout,2);
	shore_expr = aqua_tree->toShoreExpr();
	cout<< '\n';

	if (shore_expr != 0 )
	{
		if (print_oql)
			shore_expr->print_sdl();
		do_shore_q(shore_expr);
	}
	return 0;


}

void OqlInit()
{
   aqua_t::setTypePrinting(print_types);
   prompting = convert || preprint || typecheck;
}
