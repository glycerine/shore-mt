/* --------------------------------------------------------------- */
/* -- Copyright (c) 1996 Computer Sciences Department,          -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
template <class T> class expr_eval;
// we need someplace to put arp. values, consistently.
class s_value {
public:
	Ref<sdlType> vt;
	// should compress these to bit flags..
	bool byref; // sdl ref+ offset points to value.
	bool bypointer; // memory pointer to value.
	bool byvalue; // int/short/long/float, or ref as itself.
	bool tempspace; // mem. pointer needs to be freed.
	union {
		long intval;
		unsigned long uval;
		double floatval;
		char * space;
	} vu; // ignore signed/unsignet/etc.
	Ref<any> refval; // used for both byref values and ref-type values.
	void set_temp_val(Ref<sdlType> t,char *pt) 
	{ vt = t; vu.space =pt; bypointer = true; tempspace = true; }
	void set_ptr_val(Ref<sdlType> t, char *pt)
	{ vt = t; vu.space =pt; bypointer = true; tempspace = false; }
	char * get_ptr()
	{
		if (bypointer)	return vu.space;
		if (byref)	return refval.quick_eval() + vu.intval;
		if (byvalue)	return (char *)&vu;
		return 0; // unknown val??
	}
	void operator=(s_value &); // copy operator
	~s_value();
	s_value();

};
class expr_ebase {
public:
	// base class for expression evaluator classes.
	Ref<any> expr_ref; // my_ref() is typed.
	Ref<sdlType> type;
	expr_ebase *parent;
	int iter_count;

	virtual expr_eval<sdlConstDecl> * lookup_var(Ref<sdlConstDecl>);
	virtual long get_int_val(); // extract an integer & return it.
	virtual double get_float_val(); // extract a real & return it.
	virtual Ref<any> get_ref_val();
	virtual char *deref(); // really just for expr_eval_ref.
	virtual void get_val(s_value &dest ); //get a single value/initizile iteration.
	virtual bool get_first_elt(s_value &dest); // initialize iteration; return true if ok.
	virtual bool get_next_elt(s_value &dest); // continue iteration?
	// expr's can (potentially) be evaluated 2 ways- get_val will
	// set the value fields to the expression value; get_first_elt,
	// get_next_elt will iterate through a container class returning
	// the elements one at a time.
	void init() {  iter_count = 0;}

	expr_ebase(Ref<any> r, expr_ebase *ppt)
	{	expr_ref = r;  type = 0; parent = ppt; init(); }
	expr_ebase(Ref<sdlExprNode> r, expr_ebase *ppt)
	{	expr_ref = r; type = r->type; parent=ppt; init();}
	expr_ebase(Ref<sdlConstDecl> r, expr_ebase *ppt)
	{	expr_ref = r; type = r->type; parent=ppt; init();}
	void add_elt(expr_ebase *v); // dubious
	// if collectint a set, add the value in v.

};
expr_ebase * new_subeval(Ref<sdlExprNode> eref, expr_ebase *ppt);

