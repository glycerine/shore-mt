/* --------------------------------------------------------------- */
/* -- Copyright (c) 1996 Computer Sciences Department,          -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#include "expr_ebase.h"

// this template is used to allocate an expression-type specific
// eval subclass, based on our Apply parameterization. 
class get_subeval : public app_class {
public:
	expr_ebase * ept; 
	expr_ebase * ppt;

	// operator eval_base *() { return ept;};
	// this may need to be explicit...
	// override action only for the T case.
	//virtual action(T *arg); // { ept = new expr_eval<T>(arg,parent); };
	// ok, this sucks, do actions explicitly.
	virtual void action(const sdlExprNode *);
	virtual void action(const sdlLitConst *);
	virtual void action(const sdlConstName *);
	virtual void action(const sdlArithOp *);
	virtual void action(const sdlSelectExpr *);
	virtual void action(const sdlFctExpr *);
	virtual void action(const sdlConstDecl *);
	// operator (expr_ebase *)() { return ept; }
	get_subeval(Ref<sdlExprNode> eref , expr_ebase *p)
	{
		ppt = p;
		eref->Apply(this); // will set ept, through above action routine.
	}
	get_subeval(Ref<sdlConstDecl> eref , expr_ebase *p)
	{
		ppt = p;
		eref->Apply(this); // will set ept, through above action routine.
	}
};

expr_ebase * new_subeval(Ref<sdlExprNode> eref, expr_ebase *ppt);
	
expr_eval<sdlConstDecl> * new_subeval(Ref<sdlConstDecl> eref, expr_ebase *ppt);
	



#ifdef nogcc_bug
// template <class T> class expr_eval; // forward decl of explicit class
// default template for expr_eval class: just put in a ref
// to the element type.
template <class T> class expr_eval<T> : public expr_ebase
{
public:
	Ref<T> my_ref;
	expr_eval(Ref<T> me, expr_ebase *ppt)
	{ my_ref = me; parent = ppt; }
};
// gcc 2.6.3, 2.7.1 pukes on the above; so do the missing ones explicitly.
#else
#define DUMMY_EVAL(T)	\
class expr_eval<T> : public expr_ebase { public:\
	Ref<T> & my_ref() { return (Ref<T> &)expr_ref;}\
	expr_eval(Ref<T> me, expr_ebase *ppt) :expr_ebase(me,ppt){ }\
};
DUMMY_EVAL(sdlExprNode);
DUMMY_EVAL(sdlFctExpr);

#endif

class expr_eval<sdlLitConst> : public expr_ebase
{
public:
	Ref<sdlLitConst> & my_ref() { return (Ref<sdlLitConst> &)expr_ref;}
	expr_eval(Ref<sdlLitConst> me, expr_ebase *ppt)
		: expr_ebase(me,ppt){ }
	long get_int_val();
	virtual char *deref(); // really just for expr_eval_ref.
	// get pointer to value...
};


// explicit instances for ArithOp, LitConst, COnstName,...
class expr_eval<sdlArithOp> : public expr_ebase
{	public:
	// no iteration???
	Ref<sdlArithOp> &my_ref() { return (Ref<sdlArithOp> &) expr_ref; }
	expr_ebase *e1;
	expr_ebase *e2;
	// virtual void action(sdlArithOp *apt); // u
	expr_eval(Ref<sdlArithOp> aop, expr_ebase *ppt) : expr_ebase(aop,ppt)
	{
		e1 = new_subeval(aop->e1,ppt);
		e2 = new_subeval(aop->e2,ppt);
	}
};

template <class  tt,  const aqua_op a_op> 
class bin_eval_expr: public expr_ebase
{
	public:
	// no iteration???
	Ref<sdlArithOp> &my_ref() { return (Ref<sdlArithOp> &) expr_ref; }
	expr_ebase *e1;
	expr_ebase *e2;
	bin_eval_expr(Ref<sdlArithOp> aop, expr_ebase *ppt) : expr_ebase(aop,ppt)
	{
		e1 = new_subeval(aop->e1,ppt);
		e2 = new_subeval(aop->e2,ppt);
	}
	long get_int_val(); // for predicates
	double get_float_val();
	// do we need unsigned??
};

	

	
// for real ref operations, make a specific subclass
class expr_eval_ref : public expr_ebase
{	public:
	// no iteration???
	Ref<sdlArithOp> &my_ref() { return (Ref<sdlArithOp> &) expr_ref; }
	expr_ebase *base;
	Ref<sdlDeclaration> field;
	int foffset; // if it is fixed??
	// expr_ebase *field;
	// virtual void action(sdlArithOp *apt); // u
	expr_eval_ref(Ref<sdlArithOp> aop, expr_ebase *ppt);
	virtual long get_int_val(); // extract an integer & return it.
	virtual Ref<any> get_ref_val();
	virtual char *deref(); // really just for expr_eval_ref.
	virtual void get_val(s_value &dest); //get a single value/initizile iteration.
	virtual bool get_first_elt(s_value &dest); // initialize iteration; return true if ok.
	virtual bool get_next_elt(s_value &dest); // continue iteration?
};
	
// explicit instances for ArithOp, LitConst, COnstName,...
class expr_eval_aqua: public expr_ebase
{	public:
// no iteration???
	Ref<sdlArithOp> &my_ref() { return (Ref<sdlArithOp> &) expr_ref; }
	expr_ebase *e1;
	expr_ebase *e2;
	aqua_op atag;
	// virtual void action(sdlArithOp *apt); // u
	expr_eval_aqua(Ref<sdlArithOp> aop, expr_ebase *ppt) : expr_ebase(aop,ppt)
	{
		atag = aop->aop;
		e1 = new_subeval(aop->e1,ppt);
		e2 = new_subeval(aop->e2,ppt);
	}
	long get_int_val(); // for predicates

	virtual Ref<any> get_ref_val();
	virtual char *deref(); // really just for expr_eval_ref.
	virtual void get_val(s_value &dest); //get a single value/initizile iteration.
	virtual bool get_first_elt(s_value &dest); // initialize iteration; return true if ok.
	virtual bool get_next_elt(s_value &dest); // continue iteration?
};

// for union expr:
class expr_eval_aqua_union: public expr_eval_aqua
{	public:
	bool e2_active;
	// no iteration???
	// virtual void action(sdlArithOp *apt); // u
	expr_eval_aqua_union(Ref<sdlArithOp> aop, expr_ebase *ppt) 
		: expr_eval_aqua(aop,ppt) { }
	// can only be evaluated in bulk or through iterator...
	virtual void get_val(s_value &dest); 
	virtual bool get_first_elt(s_value &dest);
	virtual bool get_next_elt(s_value &dest);
};

class expr_eval<sdlConstName> : public expr_ebase
{
public:
// in this context, refers to a decl always??
	Ref<sdlConstName> &my_ref() { return (Ref<sdlConstName> &) expr_ref; }
	Ref<any> bref; // if a db variable, a ref<type>.
	expr_eval<sdlConstDecl> *ept;
// we could in principal collapse these out..
// virtual void action(app_class *apt); 
// virtual Ref<sdlConstDecl> lookup_var(char *);
	expr_eval(Ref<sdlConstName> aop, expr_ebase *ppt);
	virtual Ref<any> get_ref_val();
	virtual void get_val(s_value &dest);
	virtual char *deref(); // really just for expr_eval_ref.
};

class expr_eval<sdlSelectExpr> : public expr_ebase
{
public:
	Ref<sdlSelectExpr> & my_ref() { return (Ref<sdlSelectExpr> &) expr_ref; }
	expr_eval<sdlConstDecl> *range_eval;
	expr_ebase *proj_eval;
	expr_ebase *pred_eval;
	virtual expr_eval<sdlConstDecl> * lookup_var(Ref<sdlConstDecl> dpt);
	expr_eval(Ref<sdlSelectExpr> aop, expr_ebase *ppt);
	virtual void get_val(s_value &dest);
	virtual bool get_first_elt(s_value &dest);
	virtual bool get_next_elt(s_value &dest);
};



class expr_eval_project : public expr_ebase
{
public:
	Ref<sdlConstDecl> & my_ref() { return (Ref<sdlConstDecl> &) expr_ref; }
	int n_inits;
	expr_ebase ** init_exprs; // list of init classes.
	expr_eval_project(Ref<sdlSelectExpr> aop, expr_ebase *ppt);
	virtual void get_val(s_value &dest);
	// no iterator for now?
};
// this name can be either a range variable or a db variable
// from the namespace. look up in namespace; if not found,
// assume it is a range variable.

class expr_eval<sdlConstDecl> : public expr_ebase
{
public:
	Ref<sdlConstDecl> & my_ref() { return (Ref<sdlConstDecl> &) expr_ref; }
	expr_ebase *range_expr;
	expr_eval<sdlConstDecl> *next;
	s_value cur_val;

	expr_eval(Ref<sdlConstDecl> aop, expr_ebase *ppt); 
	virtual Ref<any> get_ref_val();
	virtual void get_val(s_value &dest); 
	//get a single value/initizile iteration.
	virtual bool get_first_elt(); 
	// initialize iteration; return true if ok.
	virtual bool get_next_elt();
	virtual expr_eval<sdlConstDecl> * lookup_var(Ref<sdlConstDecl> dpt);
	virtual char *deref();
};	// for this




#ifdef oldcode
template <class T> void get_subeval<T>::action() const
{
expr_eval<T> *ept = new expr_eval<T>(ept,parent);
return ept;
}
#endif

