/* --------------------------------------------------------------- */
/* -- Copyright (c) 1996 Computer Sciences Department,          -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
// base class for metatype applications.
// the idea is to have an object we can pass around to do various
// actions across a data type.  There is a virtual action function
// defined for each metatype object that has an apply function; 
// the defaults know the class hierarchy and will dispatch accordingly.
// this gives us an (admittedly clumsy) kind of double dispatch on the metatype
// hierachy and the action type. For the base routines, the default
// action is to invoke the action routine for the immediate supertype.
// note that we pass in memory pointers, not refs; actions are alway
// invoked from the metatype::Apply(app_class *) fct so that the 
// objects are always locked.
// note that this sucks??
class app_class {
	// expression nodes
public:
	virtual void action(const sdlExprNode *);
	virtual void action(const sdlLitConst *);
	virtual void action(const sdlConstName *);
	virtual void action(const sdlArithOp *);
	virtual void action(const sdlSelectExpr *);
	virtual void action(const sdlProjectExpr *);
	virtual void action(const sdlFctExpr *);
	// declarations
	virtual void action(const sdlDeclaration *);
	virtual void action(const sdlOpDecl *);
	virtual void action(const sdlAliasDecl *);
	virtual void action(const sdlTypeDecl *);
	virtual void action(const sdlExtTypeDecl *);
	virtual void action(const sdlConstDecl *);
	virtual void action(const sdlArmDecl *);
	virtual void action(const sdlAttrDecl *);
	virtual void action(const sdlRelDecl *);
	virtual void action(const sdlParamDecl *);
	virtual void action(const sdlModDecl *);
	// types
	virtual void action(const sdlType *);
	virtual void action(const sdlNamedType *);
	virtual void action(const sdlStructType *);
	virtual void action(const sdlEnumType *);
	virtual void action(const sdlEType *);
	virtual void action(const sdlArrayType *);
	virtual void action(const sdlSequenceType *);
	virtual void action(const sdlCollectionType *);
	virtual void action(const sdlRefType *);
	virtual void action(const sdlIndexType *);
	virtual void action(const sdlInterfaceType *);
	virtual void action(const sdlUnionType *);
	virtual void action(const sdlClassType *);
};

// this class is to be used to traverse all the data elements
// in an (in-memory) data structure.  It knows to iterate
// over attributes in interface objects, members of structs,
// the appropriate union arms, and sequence & array elements.
// The iteration will stop at "primitive types", and will
// not traverse refs.  To use this class, minimally,
// the action function for the data type to which the
// action applies should be overriden, at the lowest level
// needed.

class traverse_data: public app_class {
public:
	char * data_pt; //points to data type for current elt
	// overide composite types only; the defaults will apply
	// action to components.
	// virtual void action(const sdlType *);
	// virtual void action(const sdlNamedType *);
	virtual void action(const sdlStructType *);
	// virtual void action(const sdlEnumType *);
	// virtual void action(const sdlEType *);
	virtual void action(const sdlArrayType *);
	virtual void action(const sdlSequenceType *);
	virtual void action(const sdlCollectionType *);
	// virtual void action(const sdlRefType *);
	virtual void action(const sdlIndexType *);
	virtual void action(const sdlInterfaceType *);
	virtual void action(const sdlUnionType *);
	// virtual void action(const sdlClassType *);
};


// a bogus add on: some dumb defines in lie of metatype updates.
#define a_count a_list
#define a_sum a_array_concat

