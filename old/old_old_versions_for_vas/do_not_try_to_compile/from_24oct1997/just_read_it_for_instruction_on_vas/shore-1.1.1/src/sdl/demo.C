/* --------------------------------------------------------------- */
/* -- Copyright (c) 1996 Computer Sciences Department,          -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#include <ostream.h>
#include <strstream.h>

#include <symbol.h>
#include <types.h>
#include <typedb.h>
#include <aqua.h>

#include <aqua_names.h>

bool	aqua_t::print_types = true;

aqua_type_t::aqua_type_t(const char *name)
{
	_name = strdup(name);
	/* XXX what about defined types ? */
	/* leave for type-checking time */
	// _type = Types->force_lookup(name);
}


static char *indent(int level)
{
	static char buf[1024];
	static char spaces[] = "  ";
	char 	*s = buf;
	int	l = strlen(spaces);
	int	i;

	for (i = 0; i < level; i++) {
		strcpy(s, spaces);
		s += l;
	}
	*s = '\0';
	return buf;
}

char *aqua_t::common()
{
	static char buf[1024];
	ostrstream sbuf(buf,sizeof(buf));

	if (print_types)
		sbuf.form("\"%s\"", _type->name());
	sbuf << ends; 

	return sbuf.str();
}

ostream &aqua_t::print(ostream &s, int level)
{
	return s << "(aqua_t, op=" << _op << ", " << 
		common() << ')';
}

ostream &aqua_error_t::print(ostream &s, int level)
{
	return s << "(\"**AQUA**ERROR**\")";
}
ostream &aqua_constant_t::print(ostream &s, int level)
{
	return s << "(constant type=" << common() 
		<< " \"" << _rep << "\")";
}

ostream &aqua_dbvar_t::print(ostream &s, int level)
{
	return s << "(dbvar  \"" << _name << "\" " << common() << ")";
}

ostream &aqua_var_t::print(ostream &s, int level)
{
	return s << "(var  \"" << _name << "\" " << common() << ")";
}

ostream &aqua_use_t::print(ostream &s, int level)
{
	return s << "(use  \"" << _name << "\")";
}

ostream &aqua_type_t::print(ostream &s, int level)
{
	return s << "(type " << common() << _name.str() << " )";
}

ostream &aqua_function_t::print(ostream &s, int level)
{
	s << "(function " << common() << '\n' << indent(level+1);
	_one->print(s, level+1);
	s << '\n' << indent(level+1);
	if (_two) {
		_two->print(s, level+1);
		s << '\n' << indent(level+1);
	}
	_body->print(s, level+1);
	return s << ")";		
}

ostream &aqua_apply_t::print(ostream &s, int level)
{
	s << "(" << aqua_names[_op] << ' ' << common() << '\n'
		<< indent(level+1);
	_what->print(s, level+1);
	s << '\n' << indent(level+1);
	_to->print(s, level+1);
	return s << ")";		
}

ostream& aqua_delete_t::print(ostream& s, int level)
{
   s << "(" << aqua_names[_op] << ' ' << common() << '\n'
     << indent(level+1);
   _selector->print(s, level+1);
   s << '\n' << indent(level+1);
   _from->print(s, level+1);
   return s << ")";
}

ostream& aqua_insert_t::print(ostream& s, int level)
{
   s << "(" << aqua_names[_op] << ' ' << common() << '\n'
     << indent(level+1);
   _what->print(s, level+1);
   s << '\n' << indent(level+1);
   _into->print(s, level+1);
   return s << ")";
}

ostream& aqua_update_t::print(ostream& s, int level)
{
   s << "(" << aqua_names[_op] << ' ' << common() << '\n'
     << indent(level+1);
   _selector->print(s, level+1);
   s << '\n' << indent(level+1);
   _modifier->print(s, level+1);
   s << '\n' << indent(level+1);
   _what->print(s, level+1);
   return s << ")";
}

ostream &aqua_join_t::print(ostream &s, int level)
{
	s << "(" << aqua_names[_op] << ' ' << common() << '\n'
		<< indent(level+1);
	_what->print(s, level+1);
	s << '\n' << indent(level+1);
	_to->print(s, level+1);
	s << '\n' << indent(level+1);
	_other->print(s, level+1);
	return s << ")";		
}

ostream &aqua_zary_t::print(ostream &s, int level)
{
	return s << "(zero-ary op=" << aqua_names[_op] << " " <<
		common() << ')';
}

ostream &aqua_unary_t::print(ostream &s, int level)
{
	s << "(" << aqua_names[_op] << ' ' << common() <<
		'\n' << indent(level+1);
	_what->print(s, level+1);
	return s << ')';
}

ostream &aqua_binary_t::print(ostream &s, int level)
{
	s << "(" << aqua_names[_op] << ' ' << common() << '\n'
		<< indent(level+1);
	_left->print(s, level+1);
	s << '\n' << indent(level+1);
	_right->print(s, level+1);
	return s << ")";		
}

ostream &aqua_trinary_t::print(ostream &s, int level)
{
	s << "(" << aqua_names[_op] << '\n'
		<< indent(level+1);
	_left->print(s, level+1);
	s << '\n' << indent(level+1);
	_middle->print(s, level+1);
	s << '\n' << indent(level+1);
	_right->print(s, level+1);
	return s << ")";		
}

ostream &aqua_fold_t::print(ostream &s, int level)
{
	s << "(" << aqua_names[_op] << '\n'
		<< indent(level+1);
	_initial->print(s, level+1);
	s << '\n' << indent(level+1);
	_what->print(s, level+1);
	s << '\n' << indent(level+1);
	_folder->print(s, level+1);
	s << '\n' << indent(level+1);
	_to->print(s, level+1);
	return s << ")";		
}

ostream &aqua_tuple_t::print(ostream &s, int level)
{
	s << "(tuple \"" << _name << "\" " << common() <<
		'\n' << indent(level+1);
	_value->print(s, level+1);
	return s << ')';
}

ostream &aqua_get_t::print(ostream &s, int level)
{
	s << "(" << aqua_names[_op] << ' ' << common() <<
		'\n' << indent(level+1);
	if (!_what) _what = new aqua_error_t();
	_what->print(s, level+1);
	s << '\n' << indent(level+1);
	return s << '\"' << _name << "\")";
}

ostream &aqua_modify_tuple_t::print(ostream &s, int level)
{
	s << "(" << aqua_names[_op] << ' ' << common() <<
		'\n' << indent(level+1);
	if (!_what) _what = new aqua_error_t();
	_what->print(s, level+1);
	s << '\n' << indent(level+1);
	s << "\"" << _name << "\"" << '\n' << indent(level+1);
	_value->print(s, level + 1);
	return s << " )";
}

ostream &aqua_modify_array_t::print(ostream &s, int level)
{
	s << "(" << aqua_names[_op] << ' ' << common() <<
		'\n' << indent(level+1);
	if (!_what) _what = new aqua_error_t();
	_what->print(s, level+1);
	s << '\n' << indent(level+1);
	_at->print(s, level+1);
	s << '\n' << indent(level+1);
	_value->print(s, level + 1);
	return s << ")";
}

ostream &aqua_method_t::print(ostream &s, int level)
{
	s << "(" << aqua_names[_op] << " " << common()
		<< '\n' << indent(level+1);
	_what->print(s, level+1);
	s << '\n' << indent(level+1);
	s << "\"" << _name << "\"(" << '\n' << indent(level+1);
	_args->print(s, level+1);
	return s << ") )";
}

#ifdef TYPED_EXTENTS
ostream& aqua_closure_t::print(ostream& s, int level)
{
   s << "(" << aqua_names[_op] << " " << common()
     << '\n' << indent(level+1);
   _what->print(s, level+1);
   return s << ")";
}
#endif TYPED_EXTENTS

#if 0
ostream &aqua_assign_t::print(ostream &s, int level)
{
	s << "(assign \"" << _name << "\" := \n" << indent(level+1); 
	_what->print(s, level+1);
	return s << ")";
}
#endif


ostream &operator<<(ostream &s, aqua_t &aqua_expr)
{
	return aqua_expr.print(s);
}
