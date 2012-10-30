/* --------------------------------------------------------------- */
/* -- Copyright (c) 1996 Computer Sciences Department,          -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#ifndef aqua_names_H
#define aqua_names_H
#ifdef __cplusplus
extern "C" {
#endif
char *aqua_names[] =  {
	"nil",
	"true",
	"false",
	"constant",
	"a_var",
	"a_dbvar",
	"a_use",
	"void",
	"abs",
	"negate",
	"not",
	"a_set",
	"a_multiset",
	"a_list",
	"a_array",
	"a_multiset_to_set",
	"a_first",
	"a_last",
	"a_choose",
	"a_list_to_set",
	"a_flatten",
	"a_get",
	"a_assign",
	"a_tuple",
	"a_at",
	"a_member",
	"a_add",
	"a_concat",
	"a_subtract",
	"a_multiply",
	"a_divide",
	"a_mod",
	"a_eq",
	"a_ne",
	"a_lt",
	"a_le",
	"a_gt",
	"a_ge",
	"a_and",
	"a_or",
	"a_tuple_concat",
	"a_array_concat",
	"a_list_concat",
	"a_forall",
	"a_exists",
	"a_union",
	"a_additive_union",
	"a_cast",
	"a_sort",
	"a_min",
	"a_max",
	"a_intersect",
	"a_diff",
	"a_modify_add",
	"a_modify_subtract",
	"a_invoke",
	"a_define",
	"a_function",
	"a_mkobj",
	"a_program",
	"a_select",
	"a_group",
	"a_subrange",
	"a_tuple_join",
	"a_fold",
	"a_apply",
	"a_method",
	"a_arg",
	"a_arg_concat",
	"a_build_join",
	"a_modify",
	"a_modify_tuple",
	"a_modify_array",
	"a_update",
	"a_delete",
	"a_insert",
	"a_eval",
	"a_closure",
}; /* aqua_names */
int num_aqua_names = 78;
#ifdef __cplusplus
}
#endif

#endif
