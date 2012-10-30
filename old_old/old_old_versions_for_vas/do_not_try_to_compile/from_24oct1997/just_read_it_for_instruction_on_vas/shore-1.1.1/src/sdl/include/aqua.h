/* --------------------------------------------------------------- */
/* -- Copyright (c) 1996 Computer Sciences Department,          -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#ifndef _AQUA_H_
#define _AQUA_H_
/*
   Aqua abstract syntax tree objects
   */

/*
   Operator types

   lots
   */
#include <aqua_ops.h>
extern "C" char *strdup(const char *);

class TypeDB;
class ExtentDB;
class SymbolTable;
class oqlContext;
class typecheckState;
class translationState;

// Murali.add 4/15
class expr_t;
class exec_info_t;
class pred_t;
class funcScope_t;
class proj_list_t;

class aqua_t {
   friend ostream& operator<<(ostream &s, aqua_t &aqua_expr);
   /*
      was protected:, but 2.6 doesn't allow members to access
      protected fields of arguments of same class
      */
 public:
   Type*       _type;   // generated (or implied) data type
   aqua_op     _op;
   static bool print_types;

   char	*common();
      // synthesize type info
   virtual Type* gen(typecheckState &) {
      return _type;
   }
 public:
   aqua_t() : _type(TypeVoid) {}
   virtual ~aqua_t() {}

   virtual Ref<sdlExprNode> toShoreExpr();
   virtual Ref<sdlConstDecl> toShoreRange(); // convert to range var decl.

      // Cast this node into a new node...
//   virtual aqua_t* cast(Type* type);

   Type	*type() { return _type; }
   virtual	ostream &print(ostream &s, int level = 0);
   Type*        typecheck(oqlContext &context);
   aqua_op	op() { return _op; }
   static	void	setTypePrinting(bool onoff) { print_types = onoff; }

#ifdef USE_ALLOCATORS
   void* operator new(long sz);
   void  operator delete(void *) {}
#endif USE_ALLOCATORS
};

class aqua_error_t : public aqua_t {
 public:
   aqua_error_t() { _type = TypeError; }
   virtual	ostream &print(ostream &s, int level = 0);
};

class aqua_constant_t : public aqua_t {
 public:
   auto_string	_rep;
 public:
   aqua_constant_t(Type *type, const char *rep) 
      : _rep(rep) {_type = type;_op = a_constant;}

   virtual ostream& print(ostream &s, int level = 0);
   // expr_t* toExpr(funcScope_t& fscope);
   virtual Ref<sdlExprNode> toShoreExpr();

//   virtual aqua_t* cast(Type* type);
};

class aqua_type_t : public aqua_t {
 public:
   auto_string	_name;
 public:
   aqua_type_t(Type *type, const char *name) 
      : _name(name) {_type = type;}
   aqua_type_t(const char *name);

   virtual ostream& print(ostream &s, int level = 0);
   Type* gen(typecheckState &);
};

class aqua_dbvar_t : public aqua_t {
 public:
   auto_string	_name;
   Type	*_extent;
 public:
   aqua_dbvar_t(const char *name, Type *extent) 
      : _name(name), _extent(extent) {_op = a_dbvar;}

   virtual ostream& print(ostream &s, int level = 0);
   Type* gen(typecheckState &);
   // expr_t* toExpr(funcScope_t& fscope);
   // exec_info_t* toPPlan();
};

class aqua_var_t : public aqua_t {
 public:
   auto_string	_name;
   /* Symbol *symbol */
   aqua_var_t(/*ignored*/ aqua_op op, const char *name) 
      : _name(name){_op = a_var;}
   aqua_var_t(const char *name) 
      : _name(name) {_op = a_var;}
   
   virtual	ostream &print(ostream &s, int level = 0);
   Type *gen(typecheckState &);
};

class aqua_use_t : public aqua_t {
 public:
   auto_string	_name;
   aqua_use_t(const char *name) 
      : _name(name) {_op = a_use;}
   
   virtual	ostream &print(ostream &s, int level = 0);
   Type *gen(typecheckState &);
};

class aqua_tuple_t : public aqua_t {
 public:
   auto_string	_name;
   aqua_t	*_value;
   aqua_tuple_t(const char *name, aqua_t *value) 
      : _name(name), _value(value) { _op = a_tuple; }

   virtual ostream &print(ostream &s, int level = 0);
   Type *gen(typecheckState &);
   // aqua_t* rewrite();
};

class aqua_apply_t;

class aqua_function_t : public aqua_t {
 public:
   aqua_var_t	*_one;
   aqua_var_t	*_two;
   aqua_t       *_body;
   aqua_function_t(aqua_var_t *one, aqua_t *body) {
      _op = a_function;
      _one = one;
      _two = (aqua_var_t *)0;
      _body = body;
   }
   aqua_function_t(aqua_var_t *one, aqua_var_t *two, aqua_t *body) {
      _op = a_function;
      _one = one;
      _two = two;
      _body = body;
   }
   ostream	&print(ostream &s, int level = 0);
   Type	*gen(typecheckState &, Type *arg0 = 0, Type *arg1 = 0);

   // aqua_t* rewrite();
   // expr_t* toExpr();
   // expr_t* toExpr(funcScope_t& fscope);
   // virtual proj_list_t* toProjList();
   // virtual proj_list_t* toProjList(funcScope_t& fscope) {
      // return toProjList();
   // }
   virtual Ref<sdlExprNode> toShoreExpr();
   Ref<sdlConstDecl> toShoreProject(); // if fct is a project op...
};

/* aqua_function_user_t :-) */
class aqua_apply_t : public aqua_t {
 public:
   aqua_function_t	*_what;	/* apply this */
   aqua_t	*_to;		/* to this */

   aqua_apply_t(aqua_op op, aqua_function_t *what, aqua_t *to) {
      _op = op;
      _what = what;
      _to = to;
   }
   /* backwards compatibility */
   aqua_apply_t(aqua_op op, aqua_t *what, aqua_t *to) {
      _op = op;
      _what = (aqua_function_t *)what;
      _to = to;
   }

   ostream &print(ostream &s, int level = 0);
   Type	*gen(typecheckState &);

   // virtual aqua_t* rewrite();
   // virtual exec_info_t* toPPlan();
   // virtual expr_t*     toExpr(funcScope_t& fscope);
   virtual Ref<sdlExprNode> toShoreExpr();
   Ref<sdlConstDecl> toShoreRange(); // convert to range var decl.
};

class aqua_eval_t: public aqua_apply_t
{
 public:
   aqua_eval_t(aqua_function_t* what, aqua_t* to)
      : aqua_apply_t(a_eval, what, to) {}

   // print() is inherited...
   Type* gen(typecheckState &);
   virtual Ref<sdlExprNode> toShoreExpr();
};

#ifdef TYPED_EXTENTS
class aqua_closure_t: public aqua_t
{
 public:
   aqua_t* _what;

   aqua_closure_t(aqua_t* what): _what(what) {_op = a_closure;}

   ostream& print(ostream& s, int level = 0);
   Type* gen(typecheckState& state);
   // exec_info_t* toPPlan();
};
#endif TYPED_EXTENTS

class aqua_delete_t: public aqua_t
{
 public:
   aqua_t* _from;
   aqua_function_t* _selector;

   aqua_delete_t(aqua_t* from, aqua_function_t* predicate)
      : _from(from), _selector(predicate) {_op = a_delete;}

   ostream& print(ostream& s, int level = 0);
   virtual Type* gen(typecheckState& state);

   // aqua_t* rewrite();
   // exec_info_t* toPPlan();
};

class aqua_insert_t: public aqua_t
{
 public:
   aqua_t* _into;
   aqua_t* _what;

   aqua_insert_t(aqua_t* into, aqua_t* what)
      : _into(into), _what(what) {_op = a_insert;}

   aqua_t* closure(translationState& state);
   ostream& print(ostream& s, int level = 0);
   virtual Type* gen(typecheckState& state);

   // aqua_t*     rewrite();
   // exec_info_t* toPPlan();
};

class aqua_update_t: public aqua_t
{
 public:
   aqua_t* _what;
   aqua_function_t* _selector;
   aqua_function_t* _modifier;

   aqua_update_t(aqua_t* what, aqua_function_t* selector, 
		 aqua_function_t* modifier) 
      : _selector(selector), _modifier(modifier), _what(what) {
	 _op = a_update;
      }
   aqua_t* closure(translationState& state);
   ostream& print(ostream& s, int level = 0);
   Type* gen(typecheckState& state);

   // aqua_t*     rewrite();
   // exec_info_t* toPPlan();
};

class aqua_join_t : public aqua_apply_t {
 public:
   aqua_t	*_other;	/* other source of joined tuples */

   aqua_join_t(aqua_function_t *what, aqua_t *left, aqua_t *right) 
      : aqua_apply_t(a_tuple_join, what, left) {
      _other = right;
   }
   ostream	&print(ostream &s, int level = 0);
   Type	*gen(typecheckState &);

   // aqua_t*     rewrite();
   // exec_info_t* toPPlan();
   virtual Ref<sdlExprNode> toShoreExpr();
   Ref<sdlConstDecl> toShoreRange(); // convert to range var decl.

};

class aqua_fold_t : public aqua_apply_t {
 public:
   /* _what is treated as the extractor */
   aqua_t	*_initial;
   aqua_function_t	*_folder;
   aqua_fold_t(aqua_t *initial,
	       aqua_function_t *extractor,
	       aqua_function_t *folder,
	       aqua_t *source) : aqua_apply_t(a_fold, extractor, source) {
		  _folder = folder;
		  _initial = initial;
	       }
   virtual	ostream &print(ostream &s, int level = 0);
   Type *gen(typecheckState &);
   virtual Ref<sdlExprNode> toShoreExpr();

   // aqua_t* rewrite();
   // exec_info_t* toPPlan();
};

class aqua_trinary_t : public aqua_t {
 public:
   aqua_t	*_left;
   aqua_t	*_middle;	
   aqua_t	*_right;
   aqua_trinary_t(aqua_op op,aqua_t *left,aqua_t *middle, aqua_t *right) {
      _op = op;
      _left = left;
      _middle = middle; 
      _right = right;
   }
   virtual	ostream &print(ostream &s, int level = 0);
   Type *gen(typecheckState &);

   // virtual aqua_t* rewrite();
   // virtual expr_t* toExpr(funcScope_t& fscope);
};

class aqua_binary_t : public aqua_t {
 public:
   aqua_t	*_left;
   aqua_t	*_right;
   aqua_binary_t(aqua_op op, aqua_t *left, aqua_t *right) {
      _op = op;
      _left = left;
      _right = right;
   }
   virtual	ostream &print(ostream &s, int level = 0);
   virtual Type *gen(typecheckState &);

   // virtual aqua_t* rewrite();
   // virtual expr_t* toExpr(funcScope_t& fscope);
   // virtual proj_list_t* toProjList();
   // virtual proj_list_t* toProjList(funcScope_t& fscope);
   virtual Ref<sdlExprNode> toShoreExpr();
};

class aqua_modify_binary_t: public aqua_binary_t
{
 public:
   aqua_modify_binary_t(aqua_op op, aqua_t* left, aqua_t* right)
      : aqua_binary_t(op, left, right) {}
   // ostream& print(ostream& s, int level = 0);
   virtual Type* gen(typecheckState& state);
};

class aqua_unary_t : public aqua_t {
 public:
   aqua_t	*_what;
   aqua_unary_t(aqua_op op, aqua_t *what) {
      _op = op;
      _what = what;
   }
   virtual	ostream &print(ostream &s, int level = 0);
   virtual Type *gen(typecheckState &);

   // virtual aqua_t* rewrite();
   // virtual expr_t* toExpr(funcScope_t& fscope);
   virtual Ref<sdlExprNode> toShoreExpr();
};

class aqua_get_t : public aqua_unary_t {
 private:
   int find_offset(funcScope_t& fscope, expr_t*& expr, aqua_var_t*& var);
 public:
   auto_string	_name;
   aqua_get_t(aqua_t *what, const char *name)
      : aqua_unary_t(a_get, what), _name(name) {}
   
   virtual ostream &print(ostream &s, int level = 0);
   virtual Type	*gen(typecheckState &);

   // virtual expr_t* toExpr(funcScope_t& fscope);
   // exec_info_t* toPPlan();
   virtual Ref<sdlExprNode> toShoreExpr();
};

class aqua_modify_t: public aqua_t
{
 public:
   aqua_t* _what;
   aqua_t* _value;

   // virtual aqua_t* rewrite();
};

class aqua_modify_tuple_t: public aqua_modify_t
{
 public:
   auto_string _name;

   aqua_modify_tuple_t(aqua_t* what, const char* name, aqua_t* value)
      : _name(name) {_op = a_modify_tuple; _what = what; _value = value;}
   virtual Type* gen(typecheckState &);
   virtual ostream& print(ostream& s, int level = 0);
};

class aqua_modify_array_t: public aqua_modify_t
{
 public:
   aqua_t* _at;

   aqua_modify_array_t(aqua_t* what, aqua_t* at, aqua_t* value)
      :  _at(at) {_op = a_modify_array; _what = what; _value = value;}
   virtual Type* gen(typecheckState &);
   virtual ostream& print(ostream& s, int level = 0);

   // aqua_t* rewrite();
};

/* XXX consistency problems */
class aqua_method_t : public aqua_get_t {
 public:
   aqua_t *_args;
   int    _inquery;
   aqua_method_t(aqua_t *what, const char *method, aqua_t *args, int in_query)
      : aqua_get_t(what, method) {
	 _op = a_method;
	 _args = args;
	 _inquery = in_query;
      };
   virtual ostream &print(ostream &s, int level = 0);
   Type	*gen(typecheckState &);

   // aqua_t* rewrite();
   // expr_t* toExpr(funcScope_t& fscope);
   virtual Ref<sdlExprNode> toShoreExpr();
};

class aqua_assign_t : public aqua_binary_t {
 public:
   aqua_assign_t(aqua_t *to, aqua_t *what)
      : aqua_binary_t(a_assign, to, what) { }
   // virtual	ostream &print(ostream &s, int level = 0);
   Type	*gen(typecheckState &);
   virtual Ref<sdlExprNode> toShoreExpr();
};

class aqua_zary_t : public aqua_t {
 public:
   aqua_zary_t(aqua_op op) {_op = op;}

   virtual ostream &print(ostream &s, int level = 0);
   Type	*gen(typecheckState &);
   virtual Ref<sdlExprNode> toShoreExpr();

   // expr_t* toExpr(funcScope_t& fscope);
};

#endif /* _AQUA_H_ */ 

