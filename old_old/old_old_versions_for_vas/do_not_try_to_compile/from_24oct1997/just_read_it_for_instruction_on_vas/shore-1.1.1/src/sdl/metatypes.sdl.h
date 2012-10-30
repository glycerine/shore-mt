#ifndef metatypes_mod
#define metatypes_mod 1
#include "ShoreApp.h"
#include "aqua_ops.h"
class sdlModule_metatypes 	{ public: virtual int oid_20051(); };
static sdlModule_metatypes metatypes_header_version;
INTERFACE_PREDEFS( sdlNameScope);
INTERFACE_PREDEFS( sdlExprNode);
INTERFACE_PREDEFS1(sdlLitConst,sdlExprNode);
INTERFACE_PREDEFS1(sdlConstName,sdlExprNode);
INTERFACE_PREDEFS1(sdlArithOp,sdlExprNode);
INTERFACE_PREDEFS1(sdlSelectExpr,sdlExprNode);
INTERFACE_PREDEFS1(sdlProjectExpr,sdlExprNode);
INTERFACE_PREDEFS1(sdlFctExpr,sdlExprNode);
INTERFACE_PREDEFS1(oqlProjList,sdlExprNode);
INTERFACE_PREDEFS1(oqlExecInfo,sdlExprNode);
INTERFACE_PREDEFS1(oqlPredicate,sdlExprNode);
INTERFACE_PREDEFS( sdlDeclaration);
INTERFACE_PREDEFS1(sdlAliasDecl,sdlDeclaration);
INTERFACE_PREDEFS1(sdlTypeDecl,sdlDeclaration);
INTERFACE_PREDEFS1(sdlExtTypeDecl,sdlDeclaration);
INTERFACE_PREDEFS1(sdlConstDecl,sdlDeclaration);
INTERFACE_PREDEFS1(sdlArmDecl,sdlDeclaration);
INTERFACE_PREDEFS1(sdlAttrDecl,sdlDeclaration);
INTERFACE_PREDEFS1(sdlRelDecl,sdlAttrDecl);
INTERFACE_PREDEFS1(sdlParamDecl,sdlDeclaration);
INTERFACE_PREDEFS1(sdlModDecl,sdlDeclaration);
INTERFACE_PREDEFS1(sdlType,sdlNameScope);
INTERFACE_PREDEFS1(sdlNamedType,sdlType);
INTERFACE_PREDEFS1(sdlStructType,sdlType);
INTERFACE_PREDEFS1(sdlEnumType,sdlType);
INTERFACE_PREDEFS1(sdlEType,sdlType);
INTERFACE_PREDEFS1(sdlArrayType,sdlEType);
INTERFACE_PREDEFS1(sdlSequenceType,sdlArrayType);
INTERFACE_PREDEFS1(sdlCollectionType,sdlEType);
INTERFACE_PREDEFS1(sdlRefType,sdlEType);
INTERFACE_PREDEFS1(sdlIndexType,sdlEType);
INTERFACE_PREDEFS1(sdlOpDecl,sdlDeclaration);
INTERFACE_PREDEFS1(sdlInterfaceType,sdlType);
INTERFACE_PREDEFS1(sdlModule,sdlNameScope);
INTERFACE_PREDEFS1(sdlUnionType,sdlType);
INTERFACE_PREDEFS1(sdlClassType,sdlType);
;
;
class app_class ;
class ostream ;
typedef  LREF( char  )  CString ;
typedef  long MemberID ;
enum DeclKind {
	ERROR ,
	Constant ,
	TypeName ,
	Alias ,
	Member ,
	Arm ,
	Attribute ,
	Op ,
	OpOverride ,
	Param ,
	Mod ,
	BaseType ,
	EnumName ,
	Exception ,
	InterfaceName ,
	Relationship ,
	UnboundRelationship ,
	ImportMod ,
	ExportName ,
	ExportAll ,
	UseMod ,
	ExternType ,
	SuppressedBase };
OVERRIDE_INDVAL(DeclKind)
NO_APPLY(DeclKind)
;
enum Zone {
	Public ,
	Private ,
	Protected };
OVERRIDE_INDVAL(Zone)
NO_APPLY(Zone)
;
enum Mode {
	In ,
	Out ,
	InOut };
OVERRIDE_INDVAL(Mode)
NO_APPLY(Mode)
;
enum ExprOp {
	EError ,
	CName ,
	Literal ,
	CDefault ,
	Plus ,
	Minus ,
	Mult ,
	Div ,
	ModA ,
	Or ,
	And ,
	Xor ,
	Complement ,
	Comma ,
	LShift ,
	RShift ,
	SelectExpr ,
	ProjectList ,
	Assign ,
	Select ,
	RangeExpr ,
	RangeVar ,
	Dot };
OVERRIDE_INDVAL(ExprOp)
NO_APPLY(ExprOp)
;
enum Swap {
	None ,
	EveryTwo ,
	EveryFour ,
	Constructed };
OVERRIDE_INDVAL(Swap)
NO_APPLY(Swap)
;
enum TypeTag {
	NO_Type ,
	Sdl_any ,
	Sdl_char ,
	Sdl_short ,
	Sdl_long ,
	Sdl_float ,
	Sdl_double ,
	Sdl_boolean ,
	Sdl_octet ,
	Sdl_unsigned_short ,
	Sdl_unsigned_long ,
	Sdl_void ,
	Sdl_pool ,
	Sdl_enum ,
	Sdl_struct ,
	Sdl_union ,
	Sdl_interface ,
	Sdl_array ,
	Sdl_string ,
	Sdl_sequence ,
	Sdl_text ,
	Sdl_ref ,
	Sdl_lref ,
	Sdl_set ,
	Sdl_bag ,
	Sdl_list ,
	Sdl_multilist ,
	Sdl_NamedType ,
	Sdl_ExternType ,
	Sdl_Index ,
	Sdl_Class ,
	Sdl_CUnion };
OVERRIDE_INDVAL(TypeTag)
NO_APPLY(TypeTag)
;
 class sdlType ;
 class sdlInterfaceType ;
 class sdlModule ;
 class sdlNameScope ;
 class sdlDeclaration ;
class sdlNameScope  : public sdlObj {
COMMON_FCT_DECLS(sdlNameScope)
public:
BagInv<sdlDeclaration,sdlNameScope,4> decls
;
 Ref< sdlDeclaration  >  myDecl ;
};
class sdlExprNode  : public sdlObj {
COMMON_FCT_DECLS(sdlExprNode)
public:
 enum ExprOp etag ;
 Ref< sdlType  >  type ;
 long lineno ;
struct val_u : sdl_heap_base {
val_u(){}
val_u(const val_u & arg) { *this = arg;}
const val_u & operator=(const val_u &);
const  enum TypeTag tagval ;
int _armi( ) const;
void set_utag( enum TypeTag  _tg_val ){int old_arm =_armi();
( enum TypeTag & )tagval = _tg_val;
if (old_arm!= _armi()) __free_space();}
void set_tagval( enum TypeTag  _tg_val ){set_utag(_tg_val);}
 enum TypeTag  get_utag() const { return tagval;}
 enum TypeTag  get_tagval() const { return tagval;}
void __apply(HeapOps op);
 long   & set_long_val() {
assert(_armi()==0);
if (!space) __set_space(4);

return *( long  *   )space ;}
const  long   & get_long_val() const {
assert(_armi()==0);
return  *(const  long  *  ) space;}
 unsigned long   & set_ulong_val() {
assert(_armi()==1);
if (!space) __set_space(4);

return *( unsigned long  *   )space ;}
const  unsigned long   & get_ulong_val() const {
assert(_armi()==1);
return  *(const  unsigned long  *  ) space;}
 double   & set_double_val() {
assert(_armi()==2);
if (!space) __set_space(8);

return *( double  *   )space ;}
const  double   & get_double_val() const {
assert(_armi()==2);
return  *(const  double  *  ) space;}
 sdl_string   & set_str_val() {
assert(_armi()==3);
if (!space) __set_space(8);

return *( sdl_string  *   )space ;}
const  sdl_string   & get_str_val() const {
assert(_armi()==3);
return  *(const  sdl_string  *  ) space;}
};
;
 struct sdlExprNode::val_u tvalue ;
virtual  long fold () const ;
virtual  void  print_sdl () const ;
virtual  void  print_cxx () const ;
virtual  void  resolve_names ( Ref< sdlModule  >  p1 , Ref< sdlInterfaceType  >  p2 ) const ;
virtual  void  print_error_msg ( sdl_string msg ) const ;
virtual  void  Apply ( LREF( class app_class  )  apt ) const ;
};
class sdlLitConst :public sdlExprNode  {
COMMON_FCT_DECLS(sdlLitConst)
public:
 sdl_string imm_value ;
virtual  void  resolve_names ( Ref< sdlModule  >  p1 , Ref< sdlInterfaceType  >  p2 ) const ;
virtual  void  print_sdl () const ;
virtual  void  print_cxx () const ;
virtual  void  Apply ( LREF( class app_class  )  apt ) const ;
};
class sdlConstName :public sdlExprNode  {
COMMON_FCT_DECLS(sdlConstName)
public:
 Ref< sdlConstDecl  >  dpt ;
 sdl_string name ;
 Ref< sdlConstName  >  scope ;
virtual  void  resolve_names ( Ref< sdlModule  >  p1 , Ref< sdlInterfaceType  >  p2 ) const ;
virtual  void  print_sdl () const ;
virtual  void  print_cxx () const ;
virtual  void  Apply ( LREF( class app_class  )  apt ) const ;
};
class sdlArithOp :public sdlExprNode  {
COMMON_FCT_DECLS(sdlArithOp)
public:
 enum aqua_op aop ;
 Ref< sdlExprNode  >  e1 ;
 Ref< sdlExprNode  >  e2 ;
virtual  void  resolve_names ( Ref< sdlModule  >  p1 , Ref< sdlInterfaceType  >  p2 ) const ;
virtual  void  print_sdl () const ;
virtual  void  print_cxx () const ;
virtual  void  Apply ( LREF( class app_class  )  apt ) const ;
};
class sdlSelectExpr :public sdlExprNode  {
COMMON_FCT_DECLS(sdlSelectExpr)
public:
 Ref< sdlConstDecl  >  ProjList ;
 Ref< sdlConstDecl  >  RangeList ;
 Ref< sdlExprNode  >  Predicate ;
virtual  void  resolve_names ( Ref< sdlModule  >  p1 , Ref< sdlInterfaceType  >  p2 ) const ;
virtual  void  Apply ( LREF( class app_class  )  apt ) const ;
virtual  void  print_sdl () const ;
virtual  void  print_cxx () const ;
};
class sdlProjectExpr :public sdlExprNode  {
COMMON_FCT_DECLS(sdlProjectExpr)
public:
Ref< sdlConstDecl  > initializers;
virtual  void  print_sdl () const ;
virtual  void  Apply ( LREF( class app_class  )  apt ) const ;
};
class sdlFctExpr :public sdlExprNode  {
COMMON_FCT_DECLS(sdlFctExpr)
public:
 Ref< sdlConstName  >  arg ;
 Ref< sdlExprNode  >  body ;
virtual  void  print_sdl () const ;
virtual  void  Apply ( LREF( class app_class  )  apt ) const ;
virtual  void  resolve_names ( Ref< sdlModule  >  p1 , Ref< sdlInterfaceType  >  p2 ) const ;
};
class proj_list_t ;
class exec_info_t ;
class op_t ;
class oqlProjList :public sdlExprNode  {
COMMON_FCT_DECLS(oqlProjList)
public:
 LREF( class proj_list_t  )  proj ;
};
class oqlExecInfo :public sdlExprNode  {
COMMON_FCT_DECLS(oqlExecInfo)
public:
 LREF( class exec_info_t  )  exec ;
};
class oqlPredicate :public sdlExprNode  {
COMMON_FCT_DECLS(oqlPredicate)
public:
 LREF( class op_t  )  op ;
};
class sdlDeclaration  : public sdlObj {
COMMON_FCT_DECLS(sdlDeclaration)
public:
 Ref< sdlDeclaration  >  next ;
 sdl_string name ;
 Ref< sdlType  >  type ;
 enum DeclKind kind ;
 enum Zone zone ;
 long offset ;
 Set< Ref< sdlDeclaration  >   >  dependsOn ;
 long lineno ;
RefInv<sdlNameScope,sdlDeclaration,48> scope
;
virtual  LREF( char  )  getName ();
virtual  Ref< sdlType  >  getTypeRef ();
virtual  enum DeclKind getkind ();
virtual  void  ApplyBaseType ( Ref< sdlType  >  p1 );
virtual  Ref< sdlDeclaration  >  ListAppend ( Ref< sdlDeclaration  >  p1 );
virtual  void  resolve_names ( Ref< sdlModule  >  mscope , Ref< sdlInterfaceType  >  iscope ) const ;
virtual  void  print_sdl () const ;
virtual  void  print_cxx () const ;
virtual  void  sdl_apply ( enum HeapOps op , LREF( void  )  ptr ) const ;
virtual  void  Apply ( LREF( class app_class  )  apt ) const ;
virtual  void  print_cxx_support () const ;
virtual  void  print_access_fcts () const ;
virtual  void  print_error_msg ( sdl_string msg ) const ;
};
class sdlAliasDecl :public sdlDeclaration  {
COMMON_FCT_DECLS(sdlAliasDecl)
public:
 Ref< sdlDeclaration  >  realDecl ;
virtual  void  resolve_names ( Ref< sdlModule  >  mscope , Ref< sdlInterfaceType  >  iscope ) const ;
virtual  void  Apply ( LREF( class app_class  )  apt ) const ;
};
class sdlTypeDecl :public sdlDeclaration  {
COMMON_FCT_DECLS(sdlTypeDecl)
public:
virtual  void  resolve_names ( Ref< sdlModule  >  mscope , Ref< sdlInterfaceType  >  iscope ) const ;
virtual  void  print_cxx_support () const ;
virtual  void  Apply ( LREF( class app_class  )  apt ) const ;
};
class sdlExtTypeDecl :public sdlDeclaration  {
COMMON_FCT_DECLS(sdlExtTypeDecl)
public:
virtual  void  print_sdl () const ;
virtual  void  Apply ( LREF( class app_class  )  apt ) const ;
};
class sdlConstDecl :public sdlDeclaration  {
COMMON_FCT_DECLS(sdlConstDecl)
public:
 Ref< sdlExprNode  >  expr ;
virtual  enum Zone getZone ();
virtual  void  print_sdl () const ;
virtual  void  resolve_names ( Ref< sdlModule  >  mscope , Ref< sdlInterfaceType  >  iscope ) const ;
virtual  void  print_cxx_support () const ;
virtual  void  Apply ( LREF( class app_class  )  apt ) const ;
};
typedef  sdlTypeDecl TypeAlias ;
class sdlArmDecl :public sdlDeclaration  {
COMMON_FCT_DECLS(sdlArmDecl)
public:
 Ref< sdlExprNode  >  CaseList ;
virtual  void  print_sdl () const ;
virtual  void  resolve_names ( Ref< sdlModule  >  mscope , Ref< sdlInterfaceType  >  iscope ) const ;
virtual  void  print_access_fcts () const ;
virtual  void  Apply ( LREF( class app_class  )  apt ) const ;
};
class sdlAttrDecl :public sdlDeclaration  {
COMMON_FCT_DECLS(sdlAttrDecl)
public:
 boolean readOnly ;
 long myMid ;
 boolean Indexable ;
virtual  enum Zone getZone ();
virtual  boolean isReadOnly ();
virtual  boolean isIndexable ();
virtual  long getMid ();
virtual  void  print_sdl () const ;
virtual  void  sdl_apply ( enum HeapOps op , LREF( void  )  ptr ) const ;
virtual  void  Apply ( LREF( class app_class  )  apt ) const ;
};
class sdlRelDecl :public sdlAttrDecl  {
COMMON_FCT_DECLS(sdlRelDecl)
public:
 Ref< sdlDeclaration  >  inverseDecl ;
 Ref< sdlDeclaration  >  orderDecl ;
virtual  void  print_sdl () const ;
virtual  void  sdl_apply ( enum HeapOps op , LREF( void  )  ptr ) const ;
virtual  void  Apply ( LREF( class app_class  )  apt ) const ;
virtual  void  resolve_names ( Ref< sdlModule  >  mscope , Ref< sdlInterfaceType  >  iscope ) const ;
virtual  void  print_cxx_support () const ;
};
class sdlParamDecl :public sdlDeclaration  {
COMMON_FCT_DECLS(sdlParamDecl)
public:
 enum Mode mode ;
virtual  enum Mode getMode ();
virtual  void  setMode ( enum Mode mval );
virtual  void  print_sdl () const ;
virtual  void  Apply ( LREF( class app_class  )  apt ) const ;
};
class sdlModDecl :public sdlDeclaration  {
COMMON_FCT_DECLS(sdlModDecl)
public:
 Ref< sdlModule  >  dmodule ;
virtual  void  print_sdl () const ;
virtual  void  Apply ( LREF( class app_class  )  apt ) const ;
};
class sdlType :public sdlNameScope  {
COMMON_FCT_DECLS(sdlType)
public:
 enum TypeTag tag ;
 sdl_string name ;
 long version ;
 long size ;
 long alignment ;
 enum Swap swap ;
 Ref< sdlDeclaration  >  scope ;
 Ref< sdlType  >  bagOf ;
 Ref< sdlType  >  setOf ;
 Ref< sdlType  >  sequenceOf ;
virtual  void  print_sdl_dcl () const ;
virtual  void  print_sdl_var ( LREF( char  )  p1 ) const ;
virtual  long has_refs () const ;
virtual  long has_heapsp () const ;
virtual  void  compute_size ();
virtual  Ref< sdlType  >  resolve_names ( Ref< sdlModule  >  mscope , Ref< sdlInterfaceType  >  iscope ) const ;
virtual  void  resolve_fields ( Ref< sdlModule  >  mscope , Ref< sdlInterfaceType  >  iscope ) const ;
virtual  Ref< sdlDeclaration  >  lookup_name ( LREF( char  )  p1 , enum DeclKind p2 ) const ;
virtual  long count_fields ( enum TypeTag p1 ) const ;
virtual  void  sdl_apply ( enum HeapOps op , LREF( void  )  vpt ) const ;
virtual  void  Apply ( LREF( class app_class  )  apt ) const ;
virtual  void  print_cxx_support () const ;
virtual  boolean is_objtype () const ;
virtual  Ref< sdlType  >  get_dtype ( enum TypeTag kind ) const ;
virtual  void  print_error_msg ( sdl_string msg ) const ;
virtual  void  print_val ( LREF( class ostream  )  file , LREF( char  )  op ) const ;
};
class sdlNamedType :public sdlType  {
COMMON_FCT_DECLS(sdlNamedType)
public:
virtual  Ref< sdlType  >  resolve_names ( Ref< sdlModule  >  mscope , Ref< sdlInterfaceType  >  iscope ) const ;
virtual  void  print_error_msg ( sdl_string msg ) const ;
 long lineno ;
 Ref< sdlType  >  real_type ;
};
class sdlStructType :public sdlType  {
COMMON_FCT_DECLS(sdlStructType)
public:
 Ref< sdlDeclaration  >  members ;
virtual  Ref< sdlDeclaration  >  findMember ( LREF( char  )  p1 ) const ;
virtual  Ref< sdlDeclaration  >  memberIterator ();
virtual  void  print_sdl_dcl () const ;
virtual  void  compute_size ();
virtual  long count_fields ( enum TypeTag p1 ) const ;
virtual  void  sdl_apply ( enum HeapOps op , LREF( void  )  vpt ) const ;
virtual  void  Apply ( LREF( class app_class  )  apt ) const ;
virtual  void  resolve_fields ( Ref< sdlModule  >  mscope , Ref< sdlInterfaceType  >  iscope ) const ;
virtual  void  print_cxx_support () const ;
virtual  Ref< sdlDeclaration  >  lookup_name ( LREF( char  )  p1 , enum DeclKind p2 ) const ;
virtual  void  print_val ( LREF( class ostream  )  file , LREF( char  )  op ) const ;
};
class sdlEnumType :public sdlType  {
COMMON_FCT_DECLS(sdlEnumType)
public:
 Ref< sdlDeclaration  >  consts ;
 Ref< sdlDeclaration  >  tag_decl ;
virtual  Ref< sdlDeclaration  >  enumIterator ();
virtual  void  print_sdl_dcl () const ;
virtual  void  print_cxx_support () const ;
virtual  void  print_val ( LREF( class ostream  )  file , LREF( char  )  op ) const ;
};
class sdlEType :public sdlType  {
COMMON_FCT_DECLS(sdlEType)
public:
 Ref< sdlType  >  elementType ;
virtual  Ref< sdlType  >  getElementType () const ;
virtual  void  SetElementType ( Ref< sdlType  >  bt );
};
class sdlArrayType :public sdlEType  {
COMMON_FCT_DECLS(sdlArrayType)
public:
 long dim ;
 Ref< sdlExprNode  >  dim_expr ;
virtual  long getLength () const ;
virtual  void  print_sdl_var ( LREF( char  )  p1 ) const ;
virtual  void  compute_size ();
virtual  long count_fields ( enum TypeTag p1 ) const ;
virtual  void  sdl_apply ( enum HeapOps op , LREF( void  )  vpt ) const ;
virtual  void  Apply ( LREF( class app_class  )  apt ) const ;
virtual  Ref< sdlType  >  resolve_names ( Ref< sdlModule  >  mscope , Ref< sdlInterfaceType  >  iscope ) const ;
virtual  void  print_val ( LREF( class ostream  )  file , LREF( char  )  op ) const ;
};
class sdlSequenceType :public sdlArrayType  {
COMMON_FCT_DECLS(sdlSequenceType)
public:
virtual  void  print_sdl_var ( LREF( char  )  p1 ) const ;
virtual  void  sdl_apply ( enum HeapOps op , LREF( void  )  vpt ) const ;
virtual  void  Apply ( LREF( class app_class  )  apt ) const ;
virtual  void  print_val ( LREF( class ostream  )  file , LREF( char  )  op ) const ;
};
class sdlCollectionType :public sdlEType  {
COMMON_FCT_DECLS(sdlCollectionType)
public:
virtual  void  sdl_apply ( enum HeapOps op , LREF( void  )  vpt ) const ;
virtual  void  Apply ( LREF( class app_class  )  apt ) const ;
virtual  Ref< sdlType  >  resolve_names ( Ref< sdlModule  >  mscope , Ref< sdlInterfaceType  >  iscope ) const ;
virtual  void  print_val ( LREF( class ostream  )  file , LREF( char  )  op ) const ;
};
class sdlRefType :public sdlEType  {
COMMON_FCT_DECLS(sdlRefType)
public:
virtual  void  print_sdl_dcl () const ;
virtual  void  sdl_apply ( enum HeapOps op , LREF( void  )  vpt ) const ;
virtual  void  Apply ( LREF( class app_class  )  apt ) const ;
virtual  Ref< sdlType  >  resolve_names ( Ref< sdlModule  >  mscope , Ref< sdlInterfaceType  >  iscope ) const ;
virtual  void  print_val ( LREF( class ostream  )  file , LREF( char  )  op ) const ;
};
class sdlIndexType :public sdlEType  {
COMMON_FCT_DECLS(sdlIndexType)
public:
 Ref< sdlType  >  keyType ;
virtual  void  print_sdl_dcl () const ;
virtual  void  sdl_apply ( enum HeapOps op , LREF( void  )  vpt ) const ;
virtual  void  Apply ( LREF( class app_class  )  apt ) const ;
virtual  Ref< sdlType  >  resolve_names ( Ref< sdlModule  >  mscope , Ref< sdlInterfaceType  >  iscope ) const ;
virtual  void  print_val ( LREF( class ostream  )  file , LREF( char  )  op ) const ;
};
class sdlOpDecl :public sdlDeclaration  {
COMMON_FCT_DECLS(sdlOpDecl)
public:
 boolean isConst ;
 Ref< sdlParamDecl  >  parameters ;
virtual  Ref< sdlParamDecl  >  parametersIterator ();
virtual  void  print_sdl () const ;
virtual  void  resolve_names ( Ref< sdlModule  >  mscope , Ref< sdlInterfaceType  >  iscope ) const ;
};
class sdlInterfaceType :public sdlType  {
COMMON_FCT_DECLS(sdlInterfaceType)
public:
 Ref< sdlDeclaration  >  Bases ;
 Ref< sdlDeclaration  >  Decls ;
RefInv<sdlModule,sdlInterfaceType,72> myMod
;
 long coresize ;
 long initialHeapSize ;
 long numIndexes ;
 Ref< sdlType  >  refTo ;
virtual  Ref< sdlType  >  resolve_typename ( LREF( char  )  p1 ) const ;
virtual  Ref< sdlDeclaration  >  resolve_decl ( LREF( char  )  p1 ) const ;
virtual  Ref< sdlDeclaration  >  resolve_name ( LREF( char  )  p1 , enum DeclKind p2 ) const ;
virtual  void  print_sdl_dcl () const ;
virtual  void  print_cxx_support () const ;
virtual  void  compute_size ();
virtual  long count_fields ( enum TypeTag p1 ) const ;
virtual  void  sdl_apply ( enum HeapOps op , LREF( void  )  vpt ) const ;
virtual  void  Apply ( LREF( class app_class  )  apt ) const ;
virtual  void  resolve_fields ( Ref< sdlModule  >  mscope , Ref< sdlInterfaceType  >  iscope ) const ;
virtual  Ref< sdlDeclaration  >  lookup_name ( LREF( char  )  p1 , enum DeclKind p2 ) const ;
virtual  boolean is_objtype () const ;
virtual  Ref< sdlType  >  get_dtype ( enum TypeTag kind ) const ;
virtual  void  print_val ( LREF( class ostream  )  file , LREF( char  )  op ) const ;
};
class sdlModule :public sdlNameScope  {
COMMON_FCT_DECLS(sdlModule)
public:
 long ref_count ;
 sdl_string name ;
 sdl_string mt_version ;
 sdl_string src_file ;
 Ref< sdlDeclaration  >  export_list ;
 Ref< sdlDeclaration  >  import_list ;
 Ref< sdlDeclaration  >  decl_list ;
 Set< Ref< sdlType  >   >  Unresolved ;
 Bag< Ref< sdlDeclaration  >   >  Resolved ;
 Bag< Ref< sdlDeclaration  >   >  Unresolved_decls ;
SetInv<sdlInterfaceType,sdlModule,96> interfaces
;
 Bag< Ref< sdlDeclaration  >   >  print_list ;
 Ref< Pool   >  my_pool ;
virtual  Ref< sdlType  >  resolve_typename ( LREF( char  )  p1 ) const ;
virtual  Ref< sdlDeclaration  >  resolve_name ( LREF( char  )  p1 , enum DeclKind p2 ) const ;
virtual  void  resolve_scope ( LREF( char  )  scope , Ref< sdlModule  >  &modref , Ref< sdlInterfaceType  >  &iref ) const ;
virtual  void  resolve_types ();
virtual  void  compute_dependencies ();
virtual  void  increment_refcount ( long ref_inc );
virtual  void  print_cxx_binding () const ;
virtual  void  add_dmodules ( Set< Ref< sdlModule  >   >  &mset ) const ;
virtual  Ref< sdlType  >  add_unresolved ( Ref< sdlType  >  urname ) const ;
virtual  void  print_error_msg ( sdl_string msg ) const ;
};
class sdlUnionType :public sdlType  {
COMMON_FCT_DECLS(sdlUnionType)
public:
 Ref< sdlDeclaration  >  TagDecl ;
 Ref< sdlArmDecl  >  ArmList ;
virtual  void  print_sdl_dcl () const ;
virtual  void  compute_size ();
virtual  long count_fields ( enum TypeTag p1 ) const ;
virtual  void  sdl_apply ( enum HeapOps op , LREF( void  )  vpt ) const ;
virtual  void  Apply ( LREF( class app_class  )  apt ) const ;
virtual  void  resolve_fields ( Ref< sdlModule  >  mscope , Ref< sdlInterfaceType  >  iscope ) const ;
virtual  void  print_cxx_support () const ;
virtual  Ref< sdlDeclaration  >  lookup_name ( LREF( char  )  p1 , enum DeclKind p2 ) const ;
virtual  void  print_val ( LREF( class ostream  )  file , LREF( char  )  op ) const ;
};
class sdlClassType :public sdlType  {
COMMON_FCT_DECLS(sdlClassType)
public:
virtual  void  print_sdl_dcl () const ;
virtual  void  compute_size ();
};
#define sdlNameScope_OID ReservedSerial::_sdlNameScope.guts._low
INTERFACE_POSTDEFS(sdlNameScope)
#define sdlExprNode_OID ReservedSerial::_sdlExprNode.guts._low
INTERFACE_POSTDEFS(sdlExprNode)
#if 0
#define sdlLitConst_OID ReservedSerial::_sdlLitConst.guts._low
INTERFACE_POSTDEFS(sdlLitConst)
#define sdlConstName_OID ReservedSerial::_sdlConstName.guts._low
INTERFACE_POSTDEFS(sdlConstName)
#define sdlArithOp_OID ReservedSerial::_sdlArithOp.guts._low
INTERFACE_POSTDEFS(sdlArithOp)
#define sdlSelectExpr_OID ReservedSerial::_sdlSelectExpr.guts._low
INTERFACE_POSTDEFS(sdlSelectExpr)
#define sdlProjectExpr_OID ReservedSerial::_sdlProjectExpr.guts._low
INTERFACE_POSTDEFS(sdlProjectExpr)
#define sdlFctExpr_OID ReservedSerial::_sdlFctExpr.guts._low
INTERFACE_POSTDEFS(sdlFctExpr)
#define oqlProjList_OID ReservedSerial::_oqlProjList.guts._low
INTERFACE_POSTDEFS(oqlProjList)
#define oqlExecInfo_OID ReservedSerial::_oqlExecInfo.guts._low
INTERFACE_POSTDEFS(oqlExecInfo)
#define oqlPredicate_OID ReservedSerial::_oqlPredicate.guts._low
INTERFACE_POSTDEFS(oqlPredicate)
#else
#include "mtoids.h"
#endif

#define sdlDeclaration_OID ReservedSerial::_sdlDeclaration.guts._low
INTERFACE_POSTDEFS(sdlDeclaration)
#define sdlAliasDecl_OID ReservedSerial::_sdlAliasDecl.guts._low
INTERFACE_POSTDEFS(sdlAliasDecl)
#define sdlTypeDecl_OID ReservedSerial::_sdlTypeDecl.guts._low
INTERFACE_POSTDEFS(sdlTypeDecl)
#define sdlExtTypeDecl_OID ReservedSerial::_sdlExtTypeDecl.guts._low
INTERFACE_POSTDEFS(sdlExtTypeDecl)
#define sdlConstDecl_OID ReservedSerial::_sdlConstDecl.guts._low
INTERFACE_POSTDEFS(sdlConstDecl)
#define sdlArmDecl_OID ReservedSerial::_sdlArmDecl.guts._low
INTERFACE_POSTDEFS(sdlArmDecl)
#define sdlAttrDecl_OID ReservedSerial::_sdlAttrDecl.guts._low
INTERFACE_POSTDEFS(sdlAttrDecl)
#define sdlRelDecl_OID ReservedSerial::_sdlRelDecl.guts._low
INTERFACE_POSTDEFS(sdlRelDecl)
#define sdlParamDecl_OID ReservedSerial::_sdlParamDecl.guts._low
INTERFACE_POSTDEFS(sdlParamDecl)
#define sdlModDecl_OID ReservedSerial::_sdlModDecl.guts._low
INTERFACE_POSTDEFS(sdlModDecl)
#define sdlType_OID ReservedSerial::_sdlType.guts._low
INTERFACE_POSTDEFS(sdlType)
#define sdlNamedType_OID ReservedSerial::_sdlNamedType.guts._low
INTERFACE_POSTDEFS(sdlNamedType)
#define sdlStructType_OID ReservedSerial::_sdlStructType.guts._low
INTERFACE_POSTDEFS(sdlStructType)
#define sdlEnumType_OID ReservedSerial::_sdlEnumType.guts._low
INTERFACE_POSTDEFS(sdlEnumType)
#define sdlEType_OID ReservedSerial::_sdlEType.guts._low
INTERFACE_POSTDEFS(sdlEType)
#define sdlArrayType_OID ReservedSerial::_sdlArrayType.guts._low
INTERFACE_POSTDEFS(sdlArrayType)
#define sdlSequenceType_OID ReservedSerial::_sdlSequenceType.guts._low
INTERFACE_POSTDEFS(sdlSequenceType)
#define sdlCollectionType_OID ReservedSerial::_sdlCollectionType.guts._low
INTERFACE_POSTDEFS(sdlCollectionType)
#define sdlRefType_OID ReservedSerial::_sdlRefType.guts._low
INTERFACE_POSTDEFS(sdlRefType)
#define sdlIndexType_OID ReservedSerial::_sdlIndexType.guts._low
INTERFACE_POSTDEFS(sdlIndexType)
#define sdlOpDecl_OID ReservedSerial::_sdlOpDecl.guts._low
INTERFACE_POSTDEFS(sdlOpDecl)
#define sdlInterfaceType_OID ReservedSerial::_sdlInterfaceType.guts._low
INTERFACE_POSTDEFS(sdlInterfaceType)
#define sdlModule_OID ReservedSerial::_sdlModule.guts._low
INTERFACE_POSTDEFS(sdlModule)
#define sdlUnionType_OID ReservedSerial::_sdlUnionType.guts._low
INTERFACE_POSTDEFS(sdlUnionType)
#define sdlClassType_OID ReservedSerial::_sdlClassType.guts._low
INTERFACE_POSTDEFS(sdlClassType)

#ifdef MODULE_CODE
struct rModule metatypes("metatypes",0,10,34191 );
#define CUR_MOD metatypes
char * metatype_version_1_29 = "Header: /p/shore/shore_cvs/src/sdl/metatypes.sdl,v 1.29 1995/10/02 14:58:26 schuh Exp $
";
char * metatype_version = "1_29";
int sdlModule_metatypes::oid_20051(){ return (int)metatype_version_1_29; };
//#if 0
template class noappIndVal<DeclKind>;
template class noappIndVal<Zone>;
template class noappIndVal<Mode>;
template class noappIndVal<ExprOp>;
template class noappIndVal<Swap>;
template class noappIndVal<TypeTag>;
//#endif
SETUP_VTAB(sdlNameScope,0)
NEW_PERSISTENT(sdlNameScope)
CLASS_VIRTUALS(sdlNameScope)
template class BRef<sdlNameScope,Ref<any> >;
template class WRef<sdlNameScope>;
template class Apply<Ref<sdlNameScope> >;
template class RefPin<sdlNameScope>;
template class WRefPin<sdlNameScope>;
template class srt_type<sdlNameScope>;
TYPE(sdlNameScope) TYPE_OBJECT(sdlNameScope);
TYPE_CAST_DEF(sdlNameScope)
TYPE_CAST_END(sdlNameScope)
void sdlNameScope::__apply(HeapOps op) {
decls.__apply(op);
myDecl.__apply(op);
} // end sdlNameScope::__apply
BAG_INV_IMPL(sdlDeclaration,scope,sdlNameScope,4)
SETUP_VTAB(sdlExprNode,0)
NEW_PERSISTENT(sdlExprNode)
CLASS_VIRTUALS(sdlExprNode)
template class BRef<sdlExprNode,Ref<any> >;
template class WRef<sdlExprNode>;
template class Apply<Ref<sdlExprNode> >;
template class RefPin<sdlExprNode>;
template class WRefPin<sdlExprNode>;
template class srt_type<sdlExprNode>;
TYPE(sdlExprNode) TYPE_OBJECT(sdlExprNode);
TYPE_CAST_DEF(sdlExprNode)
TYPE_CAST_END(sdlExprNode)
void sdlExprNode::__apply(HeapOps op) {
type.__apply(op);
tvalue.__apply(op);
} // end sdlExprNode::__apply
void sdlExprNode::val_u::__apply(HeapOps op)
{
sdl_heap_base::__apply(op);
if (space != 0) switch(tagval){
case  Sdl_char :
return;
case  Sdl_short :
return;
case  Sdl_long :
return;
case  Sdl_boolean :
return;
case  Sdl_octet :
return;
case  Sdl_unsigned_short :
return;
case  Sdl_unsigned_long :
return;
case  Sdl_enum :
return;
case  Sdl_float :
return;
case  Sdl_double :
return;
case  Sdl_string :
(( sdl_string  *   )space)->__apply(op); return ;
default: return;
}}
int sdlExprNode::val_u::_armi( ) const
{ switch(tagval) {
case  Sdl_char :case  Sdl_short :case  Sdl_long :return(0);
case  Sdl_boolean :case  Sdl_octet :case  Sdl_unsigned_short :case  Sdl_unsigned_long :case  Sdl_enum :return(1);
case  Sdl_float :case  Sdl_double :return(2);
case  Sdl_string :return(3);
default: return -1;}}
const sdlExprNode::val_u & sdlExprNode::val_u::operator=(const val_u  &arg ) {
set_utag(arg.get_utag());
 switch(tagval) {
case  Sdl_char :case  Sdl_short :case  Sdl_long :set_long_val() = arg.get_long_val(); break;
case  Sdl_boolean :case  Sdl_octet :case  Sdl_unsigned_short :case  Sdl_unsigned_long :case  Sdl_enum :set_ulong_val() = arg.get_ulong_val(); break;
case  Sdl_float :case  Sdl_double :set_double_val() = arg.get_double_val(); break;
case  Sdl_string :set_str_val() = arg.get_str_val(); break;
default: sdl_heap_base::__clear();}
return *this;}
template class Apply<sdlExprNode::val_u>;
SETUP_VTAB(sdlLitConst,0)
NEW_PERSISTENT(sdlLitConst)
CLASS_VIRTUALS(sdlLitConst)
template class BRef<sdlLitConst,Ref<sdlExprNode> >;
template class WRef<sdlLitConst>;
template class Apply<Ref<sdlLitConst> >;
template class RefPin<sdlLitConst>;
template class WRefPin<sdlLitConst>;
template class srt_type<sdlLitConst>;
TYPE(sdlLitConst) TYPE_OBJECT(sdlLitConst);
TYPE_CAST_DEF(sdlLitConst)
TYPE_CAST_CASE(sdlLitConst,sdlExprNode)
TYPE_CAST_END(sdlLitConst)
void sdlLitConst::__apply(HeapOps op) {
	sdlExprNode::__apply(op);
imm_value.__apply(op);
} // end sdlLitConst::__apply
SETUP_VTAB(sdlConstName,0)
NEW_PERSISTENT(sdlConstName)
CLASS_VIRTUALS(sdlConstName)
template class BRef<sdlConstName,Ref<sdlExprNode> >;
template class WRef<sdlConstName>;
template class Apply<Ref<sdlConstName> >;
template class RefPin<sdlConstName>;
template class WRefPin<sdlConstName>;
template class srt_type<sdlConstName>;
TYPE(sdlConstName) TYPE_OBJECT(sdlConstName);
TYPE_CAST_DEF(sdlConstName)
TYPE_CAST_CASE(sdlConstName,sdlExprNode)
TYPE_CAST_END(sdlConstName)
void sdlConstName::__apply(HeapOps op) {
	sdlExprNode::__apply(op);
dpt.__apply(op);
name.__apply(op);
scope.__apply(op);
} // end sdlConstName::__apply
SETUP_VTAB(sdlArithOp,0)
NEW_PERSISTENT(sdlArithOp)
CLASS_VIRTUALS(sdlArithOp)
template class BRef<sdlArithOp,Ref<sdlExprNode> >;
template class WRef<sdlArithOp>;
template class Apply<Ref<sdlArithOp> >;
template class RefPin<sdlArithOp>;
template class WRefPin<sdlArithOp>;
template class srt_type<sdlArithOp>;
TYPE(sdlArithOp) TYPE_OBJECT(sdlArithOp);
TYPE_CAST_DEF(sdlArithOp)
TYPE_CAST_CASE(sdlArithOp,sdlExprNode)
TYPE_CAST_END(sdlArithOp)
void sdlArithOp::__apply(HeapOps op) {
	sdlExprNode::__apply(op);
e1.__apply(op);
e2.__apply(op);
} // end sdlArithOp::__apply
SETUP_VTAB(sdlSelectExpr,0)
NEW_PERSISTENT(sdlSelectExpr)
CLASS_VIRTUALS(sdlSelectExpr)
template class BRef<sdlSelectExpr,Ref<sdlExprNode> >;
template class WRef<sdlSelectExpr>;
template class Apply<Ref<sdlSelectExpr> >;
template class RefPin<sdlSelectExpr>;
template class WRefPin<sdlSelectExpr>;
template class srt_type<sdlSelectExpr>;
TYPE(sdlSelectExpr) TYPE_OBJECT(sdlSelectExpr);
TYPE_CAST_DEF(sdlSelectExpr)
TYPE_CAST_CASE(sdlSelectExpr,sdlExprNode)
TYPE_CAST_END(sdlSelectExpr)
void sdlSelectExpr::__apply(HeapOps op) {
	sdlExprNode::__apply(op);
ProjList.__apply(op);
RangeList.__apply(op);
Predicate.__apply(op);
} // end sdlSelectExpr::__apply
SETUP_VTAB(sdlProjectExpr,0)
NEW_PERSISTENT(sdlProjectExpr)
CLASS_VIRTUALS(sdlProjectExpr)
template class BRef<sdlProjectExpr,Ref<sdlExprNode> >;
template class WRef<sdlProjectExpr>;
template class Apply<Ref<sdlProjectExpr> >;
template class RefPin<sdlProjectExpr>;
template class WRefPin<sdlProjectExpr>;
template class srt_type<sdlProjectExpr>;
TYPE(sdlProjectExpr) TYPE_OBJECT(sdlProjectExpr);
TYPE_CAST_DEF(sdlProjectExpr)
TYPE_CAST_CASE(sdlProjectExpr,sdlExprNode)
TYPE_CAST_END(sdlProjectExpr)
void sdlProjectExpr::__apply(HeapOps op) {
	sdlExprNode::__apply(op);
initializers.__apply(op);
} // end sdlProjectExpr::__apply
SETUP_VTAB(sdlFctExpr,0)
NEW_PERSISTENT(sdlFctExpr)
CLASS_VIRTUALS(sdlFctExpr)
template class BRef<sdlFctExpr,Ref<sdlExprNode> >;
template class WRef<sdlFctExpr>;
template class Apply<Ref<sdlFctExpr> >;
template class RefPin<sdlFctExpr>;
template class WRefPin<sdlFctExpr>;
template class srt_type<sdlFctExpr>;
TYPE(sdlFctExpr) TYPE_OBJECT(sdlFctExpr);
TYPE_CAST_DEF(sdlFctExpr)
TYPE_CAST_CASE(sdlFctExpr,sdlExprNode)
TYPE_CAST_END(sdlFctExpr)
void sdlFctExpr::__apply(HeapOps op) {
	sdlExprNode::__apply(op);
arg.__apply(op);
body.__apply(op);
} // end sdlFctExpr::__apply
SETUP_VTAB(oqlProjList,0)
NEW_PERSISTENT(oqlProjList)
CLASS_VIRTUALS(oqlProjList)
template class BRef<oqlProjList,Ref<sdlExprNode> >;
template class WRef<oqlProjList>;
template class Apply<Ref<oqlProjList> >;
template class RefPin<oqlProjList>;
template class WRefPin<oqlProjList>;
template class srt_type<oqlProjList>;
TYPE(oqlProjList) TYPE_OBJECT(oqlProjList);
TYPE_CAST_DEF(oqlProjList)
TYPE_CAST_CASE(oqlProjList,sdlExprNode)
TYPE_CAST_END(oqlProjList)
void oqlProjList::__apply(HeapOps op) {
	sdlExprNode::__apply(op);
} // end oqlProjList::__apply
SETUP_VTAB(oqlExecInfo,0)
NEW_PERSISTENT(oqlExecInfo)
CLASS_VIRTUALS(oqlExecInfo)
template class BRef<oqlExecInfo,Ref<sdlExprNode> >;
template class WRef<oqlExecInfo>;
template class Apply<Ref<oqlExecInfo> >;
template class RefPin<oqlExecInfo>;
template class WRefPin<oqlExecInfo>;
template class srt_type<oqlExecInfo>;
TYPE(oqlExecInfo) TYPE_OBJECT(oqlExecInfo);
TYPE_CAST_DEF(oqlExecInfo)
TYPE_CAST_CASE(oqlExecInfo,sdlExprNode)
TYPE_CAST_END(oqlExecInfo)
void oqlExecInfo::__apply(HeapOps op) {
	sdlExprNode::__apply(op);
} // end oqlExecInfo::__apply
SETUP_VTAB(oqlPredicate,0)
NEW_PERSISTENT(oqlPredicate)
CLASS_VIRTUALS(oqlPredicate)
template class BRef<oqlPredicate,Ref<sdlExprNode> >;
template class WRef<oqlPredicate>;
template class Apply<Ref<oqlPredicate> >;
template class RefPin<oqlPredicate>;
template class WRefPin<oqlPredicate>;
template class srt_type<oqlPredicate>;
TYPE(oqlPredicate) TYPE_OBJECT(oqlPredicate);
TYPE_CAST_DEF(oqlPredicate)
TYPE_CAST_CASE(oqlPredicate,sdlExprNode)
TYPE_CAST_END(oqlPredicate)
void oqlPredicate::__apply(HeapOps op) {
	sdlExprNode::__apply(op);
} // end oqlPredicate::__apply
SETUP_VTAB(sdlDeclaration,0)
NEW_PERSISTENT(sdlDeclaration)
CLASS_VIRTUALS(sdlDeclaration)
template class BRef<sdlDeclaration,Ref<any> >;
template class WRef<sdlDeclaration>;
template class Apply<Ref<sdlDeclaration> >;
template class RefPin<sdlDeclaration>;
template class WRefPin<sdlDeclaration>;
template class srt_type<sdlDeclaration>;
TYPE(sdlDeclaration) TYPE_OBJECT(sdlDeclaration);
TYPE_CAST_DEF(sdlDeclaration)
TYPE_CAST_END(sdlDeclaration)
void sdlDeclaration::__apply(HeapOps op) {
next.__apply(op);
name.__apply(op);
type.__apply(op);
dependsOn.__apply(op);
scope.__apply(op);
} // end sdlDeclaration::__apply
REF_INV_IMPL(sdlNameScope,decls,sdlDeclaration,48)
SETUP_VTAB(sdlAliasDecl,0)
NEW_PERSISTENT(sdlAliasDecl)
CLASS_VIRTUALS(sdlAliasDecl)
template class BRef<sdlAliasDecl,Ref<sdlDeclaration> >;
template class WRef<sdlAliasDecl>;
template class Apply<Ref<sdlAliasDecl> >;
template class RefPin<sdlAliasDecl>;
template class WRefPin<sdlAliasDecl>;
template class srt_type<sdlAliasDecl>;
TYPE(sdlAliasDecl) TYPE_OBJECT(sdlAliasDecl);
TYPE_CAST_DEF(sdlAliasDecl)
TYPE_CAST_CASE(sdlAliasDecl,sdlDeclaration)
TYPE_CAST_END(sdlAliasDecl)
void sdlAliasDecl::__apply(HeapOps op) {
	sdlDeclaration::__apply(op);
realDecl.__apply(op);
} // end sdlAliasDecl::__apply
SETUP_VTAB(sdlTypeDecl,0)
NEW_PERSISTENT(sdlTypeDecl)
CLASS_VIRTUALS(sdlTypeDecl)
template class BRef<sdlTypeDecl,Ref<sdlDeclaration> >;
template class WRef<sdlTypeDecl>;
template class Apply<Ref<sdlTypeDecl> >;
template class RefPin<sdlTypeDecl>;
template class WRefPin<sdlTypeDecl>;
template class srt_type<sdlTypeDecl>;
TYPE(sdlTypeDecl) TYPE_OBJECT(sdlTypeDecl);
TYPE_CAST_DEF(sdlTypeDecl)
TYPE_CAST_CASE(sdlTypeDecl,sdlDeclaration)
TYPE_CAST_END(sdlTypeDecl)
void sdlTypeDecl::__apply(HeapOps op) {
	sdlDeclaration::__apply(op);
} // end sdlTypeDecl::__apply
SETUP_VTAB(sdlExtTypeDecl,0)
NEW_PERSISTENT(sdlExtTypeDecl)
CLASS_VIRTUALS(sdlExtTypeDecl)
template class BRef<sdlExtTypeDecl,Ref<sdlDeclaration> >;
template class WRef<sdlExtTypeDecl>;
template class Apply<Ref<sdlExtTypeDecl> >;
template class RefPin<sdlExtTypeDecl>;
template class WRefPin<sdlExtTypeDecl>;
template class srt_type<sdlExtTypeDecl>;
TYPE(sdlExtTypeDecl) TYPE_OBJECT(sdlExtTypeDecl);
TYPE_CAST_DEF(sdlExtTypeDecl)
TYPE_CAST_CASE(sdlExtTypeDecl,sdlDeclaration)
TYPE_CAST_END(sdlExtTypeDecl)
void sdlExtTypeDecl::__apply(HeapOps op) {
	sdlDeclaration::__apply(op);
} // end sdlExtTypeDecl::__apply
SETUP_VTAB(sdlConstDecl,0)
NEW_PERSISTENT(sdlConstDecl)
CLASS_VIRTUALS(sdlConstDecl)
template class BRef<sdlConstDecl,Ref<sdlDeclaration> >;
template class WRef<sdlConstDecl>;
template class Apply<Ref<sdlConstDecl> >;
template class RefPin<sdlConstDecl>;
template class WRefPin<sdlConstDecl>;
template class srt_type<sdlConstDecl>;
TYPE(sdlConstDecl) TYPE_OBJECT(sdlConstDecl);
TYPE_CAST_DEF(sdlConstDecl)
TYPE_CAST_CASE(sdlConstDecl,sdlDeclaration)
TYPE_CAST_END(sdlConstDecl)
void sdlConstDecl::__apply(HeapOps op) {
	sdlDeclaration::__apply(op);
expr.__apply(op);
} // end sdlConstDecl::__apply
SETUP_VTAB(sdlArmDecl,0)
NEW_PERSISTENT(sdlArmDecl)
CLASS_VIRTUALS(sdlArmDecl)
template class BRef<sdlArmDecl,Ref<sdlDeclaration> >;
template class WRef<sdlArmDecl>;
template class Apply<Ref<sdlArmDecl> >;
template class RefPin<sdlArmDecl>;
template class WRefPin<sdlArmDecl>;
template class srt_type<sdlArmDecl>;
TYPE(sdlArmDecl) TYPE_OBJECT(sdlArmDecl);
TYPE_CAST_DEF(sdlArmDecl)
TYPE_CAST_CASE(sdlArmDecl,sdlDeclaration)
TYPE_CAST_END(sdlArmDecl)
void sdlArmDecl::__apply(HeapOps op) {
	sdlDeclaration::__apply(op);
CaseList.__apply(op);
} // end sdlArmDecl::__apply
SETUP_VTAB(sdlAttrDecl,0)
NEW_PERSISTENT(sdlAttrDecl)
CLASS_VIRTUALS(sdlAttrDecl)
template class BRef<sdlAttrDecl,Ref<sdlDeclaration> >;
template class WRef<sdlAttrDecl>;
template class Apply<Ref<sdlAttrDecl> >;
template class RefPin<sdlAttrDecl>;
template class WRefPin<sdlAttrDecl>;
template class srt_type<sdlAttrDecl>;
TYPE(sdlAttrDecl) TYPE_OBJECT(sdlAttrDecl);
TYPE_CAST_DEF(sdlAttrDecl)
TYPE_CAST_CASE(sdlAttrDecl,sdlDeclaration)
TYPE_CAST_END(sdlAttrDecl)
void sdlAttrDecl::__apply(HeapOps op) {
	sdlDeclaration::__apply(op);
} // end sdlAttrDecl::__apply
SETUP_VTAB(sdlRelDecl,0)
NEW_PERSISTENT(sdlRelDecl)
CLASS_VIRTUALS(sdlRelDecl)
template class BRef<sdlRelDecl,Ref<sdlAttrDecl> >;
template class WRef<sdlRelDecl>;
template class Apply<Ref<sdlRelDecl> >;
template class RefPin<sdlRelDecl>;
template class WRefPin<sdlRelDecl>;
template class srt_type<sdlRelDecl>;
TYPE(sdlRelDecl) TYPE_OBJECT(sdlRelDecl);
TYPE_CAST_DEF(sdlRelDecl)
TYPE_CAST_CASE(sdlRelDecl,sdlAttrDecl)
TYPE_CAST_END(sdlRelDecl)
void sdlRelDecl::__apply(HeapOps op) {
	sdlAttrDecl::__apply(op);
inverseDecl.__apply(op);
orderDecl.__apply(op);
} // end sdlRelDecl::__apply
SETUP_VTAB(sdlParamDecl,0)
NEW_PERSISTENT(sdlParamDecl)
CLASS_VIRTUALS(sdlParamDecl)
template class BRef<sdlParamDecl,Ref<sdlDeclaration> >;
template class WRef<sdlParamDecl>;
template class Apply<Ref<sdlParamDecl> >;
template class RefPin<sdlParamDecl>;
template class WRefPin<sdlParamDecl>;
template class srt_type<sdlParamDecl>;
TYPE(sdlParamDecl) TYPE_OBJECT(sdlParamDecl);
TYPE_CAST_DEF(sdlParamDecl)
TYPE_CAST_CASE(sdlParamDecl,sdlDeclaration)
TYPE_CAST_END(sdlParamDecl)
void sdlParamDecl::__apply(HeapOps op) {
	sdlDeclaration::__apply(op);
} // end sdlParamDecl::__apply
SETUP_VTAB(sdlModDecl,0)
NEW_PERSISTENT(sdlModDecl)
CLASS_VIRTUALS(sdlModDecl)
template class BRef<sdlModDecl,Ref<sdlDeclaration> >;
template class WRef<sdlModDecl>;
template class Apply<Ref<sdlModDecl> >;
template class RefPin<sdlModDecl>;
template class WRefPin<sdlModDecl>;
template class srt_type<sdlModDecl>;
TYPE(sdlModDecl) TYPE_OBJECT(sdlModDecl);
TYPE_CAST_DEF(sdlModDecl)
TYPE_CAST_CASE(sdlModDecl,sdlDeclaration)
TYPE_CAST_END(sdlModDecl)
void sdlModDecl::__apply(HeapOps op) {
	sdlDeclaration::__apply(op);
dmodule.__apply(op);
} // end sdlModDecl::__apply
SETUP_VTAB(sdlType,0)
NEW_PERSISTENT(sdlType)
CLASS_VIRTUALS(sdlType)
template class BRef<sdlType,Ref<sdlNameScope> >;
template class WRef<sdlType>;
template class Apply<Ref<sdlType> >;
template class RefPin<sdlType>;
template class WRefPin<sdlType>;
template class srt_type<sdlType>;
TYPE(sdlType) TYPE_OBJECT(sdlType);
TYPE_CAST_DEF(sdlType)
TYPE_CAST_CASE(sdlType,sdlNameScope)
TYPE_CAST_END(sdlType)
void sdlType::__apply(HeapOps op) {
	sdlNameScope::__apply(op);
name.__apply(op);
scope.__apply(op);
bagOf.__apply(op);
setOf.__apply(op);
sequenceOf.__apply(op);
} // end sdlType::__apply
SETUP_VTAB(sdlNamedType,0)
NEW_PERSISTENT(sdlNamedType)
CLASS_VIRTUALS(sdlNamedType)
template class BRef<sdlNamedType,Ref<sdlType> >;
template class WRef<sdlNamedType>;
template class Apply<Ref<sdlNamedType> >;
template class RefPin<sdlNamedType>;
template class WRefPin<sdlNamedType>;
template class srt_type<sdlNamedType>;
TYPE(sdlNamedType) TYPE_OBJECT(sdlNamedType);
TYPE_CAST_DEF(sdlNamedType)
TYPE_CAST_CASE(sdlNamedType,sdlType)
TYPE_CAST_END(sdlNamedType)
void sdlNamedType::__apply(HeapOps op) {
	sdlType::__apply(op);
real_type.__apply(op);
} // end sdlNamedType::__apply
SETUP_VTAB(sdlStructType,0)
NEW_PERSISTENT(sdlStructType)
CLASS_VIRTUALS(sdlStructType)
template class BRef<sdlStructType,Ref<sdlType> >;
template class WRef<sdlStructType>;
template class Apply<Ref<sdlStructType> >;
template class RefPin<sdlStructType>;
template class WRefPin<sdlStructType>;
template class srt_type<sdlStructType>;
TYPE(sdlStructType) TYPE_OBJECT(sdlStructType);
TYPE_CAST_DEF(sdlStructType)
TYPE_CAST_CASE(sdlStructType,sdlType)
TYPE_CAST_END(sdlStructType)
void sdlStructType::__apply(HeapOps op) {
	sdlType::__apply(op);
members.__apply(op);
} // end sdlStructType::__apply
SETUP_VTAB(sdlEnumType,0)
NEW_PERSISTENT(sdlEnumType)
CLASS_VIRTUALS(sdlEnumType)
template class BRef<sdlEnumType,Ref<sdlType> >;
template class WRef<sdlEnumType>;
template class Apply<Ref<sdlEnumType> >;
template class RefPin<sdlEnumType>;
template class WRefPin<sdlEnumType>;
template class srt_type<sdlEnumType>;
TYPE(sdlEnumType) TYPE_OBJECT(sdlEnumType);
TYPE_CAST_DEF(sdlEnumType)
TYPE_CAST_CASE(sdlEnumType,sdlType)
TYPE_CAST_END(sdlEnumType)
void sdlEnumType::__apply(HeapOps op) {
	sdlType::__apply(op);
consts.__apply(op);
tag_decl.__apply(op);
} // end sdlEnumType::__apply
SETUP_VTAB(sdlEType,0)
NEW_PERSISTENT(sdlEType)
CLASS_VIRTUALS(sdlEType)
template class BRef<sdlEType,Ref<sdlType> >;
template class WRef<sdlEType>;
template class Apply<Ref<sdlEType> >;
template class RefPin<sdlEType>;
template class WRefPin<sdlEType>;
template class srt_type<sdlEType>;
TYPE(sdlEType) TYPE_OBJECT(sdlEType);
TYPE_CAST_DEF(sdlEType)
TYPE_CAST_CASE(sdlEType,sdlType)
TYPE_CAST_END(sdlEType)
void sdlEType::__apply(HeapOps op) {
	sdlType::__apply(op);
elementType.__apply(op);
} // end sdlEType::__apply
SETUP_VTAB(sdlArrayType,0)
NEW_PERSISTENT(sdlArrayType)
CLASS_VIRTUALS(sdlArrayType)
template class BRef<sdlArrayType,Ref<sdlEType> >;
template class WRef<sdlArrayType>;
template class Apply<Ref<sdlArrayType> >;
template class RefPin<sdlArrayType>;
template class WRefPin<sdlArrayType>;
template class srt_type<sdlArrayType>;
TYPE(sdlArrayType) TYPE_OBJECT(sdlArrayType);
TYPE_CAST_DEF(sdlArrayType)
TYPE_CAST_CASE(sdlArrayType,sdlEType)
TYPE_CAST_END(sdlArrayType)
void sdlArrayType::__apply(HeapOps op) {
	sdlEType::__apply(op);
dim_expr.__apply(op);
} // end sdlArrayType::__apply
SETUP_VTAB(sdlSequenceType,0)
NEW_PERSISTENT(sdlSequenceType)
CLASS_VIRTUALS(sdlSequenceType)
template class BRef<sdlSequenceType,Ref<sdlArrayType> >;
template class WRef<sdlSequenceType>;
template class Apply<Ref<sdlSequenceType> >;
template class RefPin<sdlSequenceType>;
template class WRefPin<sdlSequenceType>;
template class srt_type<sdlSequenceType>;
TYPE(sdlSequenceType) TYPE_OBJECT(sdlSequenceType);
TYPE_CAST_DEF(sdlSequenceType)
TYPE_CAST_CASE(sdlSequenceType,sdlArrayType)
TYPE_CAST_END(sdlSequenceType)
void sdlSequenceType::__apply(HeapOps op) {
	sdlArrayType::__apply(op);
} // end sdlSequenceType::__apply
SETUP_VTAB(sdlCollectionType,0)
NEW_PERSISTENT(sdlCollectionType)
CLASS_VIRTUALS(sdlCollectionType)
template class BRef<sdlCollectionType,Ref<sdlEType> >;
template class WRef<sdlCollectionType>;
template class Apply<Ref<sdlCollectionType> >;
template class RefPin<sdlCollectionType>;
template class WRefPin<sdlCollectionType>;
template class srt_type<sdlCollectionType>;
TYPE(sdlCollectionType) TYPE_OBJECT(sdlCollectionType);
TYPE_CAST_DEF(sdlCollectionType)
TYPE_CAST_CASE(sdlCollectionType,sdlEType)
TYPE_CAST_END(sdlCollectionType)
void sdlCollectionType::__apply(HeapOps op) {
	sdlEType::__apply(op);
} // end sdlCollectionType::__apply
SETUP_VTAB(sdlRefType,0)
NEW_PERSISTENT(sdlRefType)
CLASS_VIRTUALS(sdlRefType)
template class BRef<sdlRefType,Ref<sdlEType> >;
template class WRef<sdlRefType>;
template class Apply<Ref<sdlRefType> >;
template class RefPin<sdlRefType>;
template class WRefPin<sdlRefType>;
template class srt_type<sdlRefType>;
TYPE(sdlRefType) TYPE_OBJECT(sdlRefType);
TYPE_CAST_DEF(sdlRefType)
TYPE_CAST_CASE(sdlRefType,sdlEType)
TYPE_CAST_END(sdlRefType)
void sdlRefType::__apply(HeapOps op) {
	sdlEType::__apply(op);
} // end sdlRefType::__apply
SETUP_VTAB(sdlIndexType,0)
NEW_PERSISTENT(sdlIndexType)
CLASS_VIRTUALS(sdlIndexType)
template class BRef<sdlIndexType,Ref<sdlEType> >;
template class WRef<sdlIndexType>;
template class Apply<Ref<sdlIndexType> >;
template class RefPin<sdlIndexType>;
template class WRefPin<sdlIndexType>;
template class srt_type<sdlIndexType>;
TYPE(sdlIndexType) TYPE_OBJECT(sdlIndexType);
TYPE_CAST_DEF(sdlIndexType)
TYPE_CAST_CASE(sdlIndexType,sdlEType)
TYPE_CAST_END(sdlIndexType)
void sdlIndexType::__apply(HeapOps op) {
	sdlEType::__apply(op);
keyType.__apply(op);
} // end sdlIndexType::__apply
SETUP_VTAB(sdlOpDecl,0)
NEW_PERSISTENT(sdlOpDecl)
CLASS_VIRTUALS(sdlOpDecl)
template class BRef<sdlOpDecl,Ref<sdlDeclaration> >;
template class WRef<sdlOpDecl>;
template class Apply<Ref<sdlOpDecl> >;
template class RefPin<sdlOpDecl>;
template class WRefPin<sdlOpDecl>;
template class srt_type<sdlOpDecl>;
TYPE(sdlOpDecl) TYPE_OBJECT(sdlOpDecl);
TYPE_CAST_DEF(sdlOpDecl)
TYPE_CAST_CASE(sdlOpDecl,sdlDeclaration)
TYPE_CAST_END(sdlOpDecl)
void sdlOpDecl::__apply(HeapOps op) {
	sdlDeclaration::__apply(op);
parameters.__apply(op);
} // end sdlOpDecl::__apply
SETUP_VTAB(sdlInterfaceType,0)
NEW_PERSISTENT(sdlInterfaceType)
CLASS_VIRTUALS(sdlInterfaceType)
template class BRef<sdlInterfaceType,Ref<sdlType> >;
template class WRef<sdlInterfaceType>;
template class Apply<Ref<sdlInterfaceType> >;
template class RefPin<sdlInterfaceType>;
template class WRefPin<sdlInterfaceType>;
template class srt_type<sdlInterfaceType>;
TYPE(sdlInterfaceType) TYPE_OBJECT(sdlInterfaceType);
TYPE_CAST_DEF(sdlInterfaceType)
TYPE_CAST_CASE(sdlInterfaceType,sdlType)
TYPE_CAST_END(sdlInterfaceType)
void sdlInterfaceType::__apply(HeapOps op) {
	sdlType::__apply(op);
Bases.__apply(op);
Decls.__apply(op);
myMod.__apply(op);
refTo.__apply(op);
} // end sdlInterfaceType::__apply
REF_INV_IMPL(sdlModule,interfaces,sdlInterfaceType,72)
SETUP_VTAB(sdlModule,0)
NEW_PERSISTENT(sdlModule)
CLASS_VIRTUALS(sdlModule)
template class BRef<sdlModule,Ref<sdlNameScope> >;
template class WRef<sdlModule>;
template class Apply<Ref<sdlModule> >;
template class RefPin<sdlModule>;
template class WRefPin<sdlModule>;
template class srt_type<sdlModule>;
TYPE(sdlModule) TYPE_OBJECT(sdlModule);
TYPE_CAST_DEF(sdlModule)
TYPE_CAST_CASE(sdlModule,sdlNameScope)
TYPE_CAST_END(sdlModule)
void sdlModule::__apply(HeapOps op) {
	sdlNameScope::__apply(op);
name.__apply(op);
mt_version.__apply(op);
src_file.__apply(op);
export_list.__apply(op);
import_list.__apply(op);
decl_list.__apply(op);
Unresolved.__apply(op);
Resolved.__apply(op);
Unresolved_decls.__apply(op);
interfaces.__apply(op);
print_list.__apply(op);
my_pool.__apply(op);
} // end sdlModule::__apply
SET_INV_IMPL(sdlInterfaceType,myMod,sdlModule,96)
SETUP_VTAB(sdlUnionType,0)
NEW_PERSISTENT(sdlUnionType)
CLASS_VIRTUALS(sdlUnionType)
template class BRef<sdlUnionType,Ref<sdlType> >;
template class WRef<sdlUnionType>;
template class Apply<Ref<sdlUnionType> >;
template class RefPin<sdlUnionType>;
template class WRefPin<sdlUnionType>;
template class srt_type<sdlUnionType>;
TYPE(sdlUnionType) TYPE_OBJECT(sdlUnionType);
TYPE_CAST_DEF(sdlUnionType)
TYPE_CAST_CASE(sdlUnionType,sdlType)
TYPE_CAST_END(sdlUnionType)
void sdlUnionType::__apply(HeapOps op) {
	sdlType::__apply(op);
TagDecl.__apply(op);
ArmList.__apply(op);
} // end sdlUnionType::__apply
SETUP_VTAB(sdlClassType,0)
NEW_PERSISTENT(sdlClassType)
CLASS_VIRTUALS(sdlClassType)
template class BRef<sdlClassType,Ref<sdlType> >;
template class WRef<sdlClassType>;
template class Apply<Ref<sdlClassType> >;
template class RefPin<sdlClassType>;
template class WRefPin<sdlClassType>;
template class srt_type<sdlClassType>;
TYPE(sdlClassType) TYPE_OBJECT(sdlClassType);
TYPE_CAST_DEF(sdlClassType)
TYPE_CAST_CASE(sdlClassType,sdlType)
TYPE_CAST_END(sdlClassType)
void sdlClassType::__apply(HeapOps op) {
	sdlType::__apply(op);
} // end sdlClassType::__apply
#undef CUR_MOD metatypes
template class  Bag< Ref< sdlDeclaration  >   >  ;
 template class  Set< Ref< sdlDeclaration  >   >  ;
 template class Sequence< Ref< sdlDeclaration  >    > ;
template class Sequence< Ref< sdlExprNode  >    > ;
template class  Set< Ref< sdlType  >   >  ;
 template class Sequence< Ref< sdlType  >    > ;
template class  Set< Ref< sdlInterfaceType  >   >  ;
 template class Sequence< Ref< sdlInterfaceType  >    > ;
template class  Set< Ref< sdlModule  >   >  ;
 template class Sequence< Ref< sdlModule  >    > ;

#endif MODULE_CODE
#endif metatypes_mod
