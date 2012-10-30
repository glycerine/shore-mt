#ifndef _TREE_H_
#define _TREE_H_
/* --------------------------------------------------------------- */
/* -- Copyright (c) 1996 Computer Sciences Department,          -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#include <node_types.h>
#include <symbol.h>
#include <types.h>
#include <aqua.h>

/** Murali.add 2/16/95 **/
#include <m_list.h>

class oqlContext;
class translationState;

typedef enum node_type NodeType;
class   Ql_tree_node;

class  Ql_tree_node {
   friend ostream &operator<<(ostream &s, Ql_tree_node *node);
   friend ostream &operator<<(ostream &s, Ql_tree_node &node);

   // private:
 public:

   NodeType	_type;
//   m_list_t<Ql_tree_node> _kids;    // A list of kids
      // Added 2/16/95
   Ql_tree_node **_kids;
   int	        _nkids;
   int          _size;
   auto_string	_id;
   Ql_tree_node* top() {return _nkids ? _kids[0] : 0;}
   Ql_tree_node* bottom() {return _nkids ? _kids[_nkids-1] : 0;}
   void	        init(NodeType type, int nkids);
   void         setType(NodeType type) {_type = type;}
   NodeType     type() {return _type;}
   ostream	&print(ostream &s, int level = 0);

   // char	*steal();	// dups the id
   const char	*id()   { return _id; }

#ifdef USE_ALLOCATORS
   void* operator new(long sz);
   void  operator delete(void *) {}
#endif USE_ALLOCATORS

   aqua_t	*convert(translationState &);
   aqua_t	*convert_zary(translationState &);
   aqua_t	*convert_trinary(translationState &);
   aqua_t	*convert_binary(translationState &);
   aqua_t	*convert_unary(translationState &);
   aqua_t	*convert_constant(translationState &);
   aqua_t	*convert_list(translationState &);
   aqua_t	*convert_select(translationState &);
   aqua_t	*convert_object(translationState &);
   aqua_t	*convert_one_scan_var(translationState &);
   aqua_t	*convert_id(translationState &);
   aqua_t	*convert_define(translationState &);
   aqua_t	*convert_existential(translationState &);
   aqua_t	*expand_lambda(Symbol *sym);
   aqua_t	*convert_collection(translationState &);
   aqua_t	*convert_parameters(translationState &);
   aqua_t	*convert_sort(translationState &);
   aqua_t	*convert_group(translationState &);
   aqua_t	*convert_folded(translationState &);	/* XXX right word */
   aqua_t	*convert_method(translationState &);

// Murali.add 3/4
   aqua_t* convert_delete(translationState &);
   aqua_t* convert_insert(translationState &);
   aqua_t* convert_update(translationState &);
   aqua_t* convert_update_expr_list(translationState &);
   aqua_t* convert_assign(translationState &);
   aqua_t* convert_closure(translationState &);
   aqua_t* convert_update_closure(translationState &);
   aqua_t* convert_create(translationState &);

   // direct conversion to sdl metatypes exprs...
   Ref<sdlExprNode>	sconvert(translationState &);
   Ref<sdlExprNode>	sconvert_zary(translationState &);
   Ref<sdlExprNode>	sconvert_trinary(translationState &);
   Ref<sdlExprNode>	sconvert_binary(translationState &);
   Ref<sdlExprNode>	sconvert_unary(translationState &);
   Ref<sdlExprNode>	sconvert_constant(translationState &);
   Ref<sdlExprNode>	sconvert_list(translationState &);
   Ref<sdlExprNode>	sconvert_select(translationState &);
   Ref<sdlExprNode>	sconvert_object(translationState &);
   Ref<sdlConstDecl>	sconvert_one_scan_var(translationState &);
   Ref<sdlExprNode>	sconvert_id(translationState &);
   Ref<sdlExprNode>	sconvert_define(translationState &);
   Ref<sdlExprNode>	sconvert_existential(translationState &);
   Ref<sdlExprNode>	sconvert_collection(translationState &);
   Ref<sdlConstDecl>	sconvert_parameters(translationState &);
   Ref<sdlExprNode>	sconvert_sort(translationState &);
   Ref<sdlExprNode>	sconvert_group(translationState &);
   Ref<sdlExprNode>	sconvert_folded(translationState &);	/* XXX right word */
   Ref<sdlExprNode>	sconvert_method(translationState &);

// Murali.add 3/4
   Ref<sdlExprNode> sconvert_delete(translationState &);
   Ref<sdlExprNode>  sconvert_insert(translationState &);
   Ref<sdlExprNode>  sconvert_update(translationState &);
   Ref<sdlExprNode>  sconvert_update_expr_list(translationState &);
   Ref<sdlExprNode>  sconvert_assign(translationState &);
   Ref<sdlExprNode>  sconvert_closure(translationState &);
   Ref<sdlExprNode>  sconvert_update_closure(translationState &);
   Ref<sdlExprNode>  sconvert_create(translationState &);
   Ref<sdlConstDecl> sconvert_dlist(translationState &);
   Ref<sdlConstDecl> sconvert_decl(translationState &);


   void	enter_symbols(translationState &);

 public:
   ~Ql_tree_node();
   Ql_tree_node(NodeType type, const char *id);
   Ql_tree_node(NodeType type, int nkids = 0);
   Ql_tree_node(NodeType type, int nkids,
		Ql_tree_node *one,
		Ql_tree_node *two = 0, 
		Ql_tree_node *three = 0,
		Ql_tree_node *four = 0);
   Ql_tree_node* add(Ql_tree_node* what);
   aqua_t*       aqua_convert(oqlContext &context);
   Ref<sdlExprNode>       shore_convert(oqlContext &context);
};

extern char *node_name(NodeType nt);
#endif /* _TREE_H_ */

