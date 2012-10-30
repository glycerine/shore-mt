/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#include "sdl_internal.h"
#include "sdl_lookup.h"

char * module_path = "/types";

sdl_scope::sdl_scope()
{
	num_mods = 0;
}

REF(Type)
sdl_scope::lookup(char *iname)
// for each module in scope list, look for an "interface" class with 
// the given name.
{
	int i;
	for (i = num_mods -1; i>=0; --i)
	{
		REF(Type) tpt;
		REF(Module) mpt = modules[i];
		if (mpt == 0) continue;
		tpt = mpt->resolve_typename(iname);
		if (tpt!= 0)
			return tpt;
	}
	return 0;
}

void
sdl_scope::push(REF(Module) mpt)
{
	if (num_mods+1 < MAX_MODS)
	{
		modules[num_mods] = mpt;
		++num_mods;
	}
}

void
sdl_scope::push(char *mname)
// push given a registered object name
{
	REF(Module) mpt;
	W_COERCE(REF(Module)::lookup(mname, mpt));
	// we allow a null name to be pushed, in order to 
	// make push/pop match
	push(mpt);
}

void
sdl_scope::pop()
{
	if (num_mods > 0)
		--num_mods;
}

REF(Declaration)
lookup_field(REF(Type) &tpt, char *fname, int &offset)
// look for a field with the given name in the
// type; return an offset and a declaration if found.
// there are really only 2 valid types here; they need to be handled
// a little differently.  The offset field is always relative
// to a pointer of type tpt; this initial version does not handle
// multiple inheritance properly; it will need to be fixed.
{
	REF(Declaration) rpt;
	if (tpt == 0) return 0;
	TypeTag tptag = tpt->tag;
	switch(tptag)
	{
		case Sdl_interface:
		{
			REF(InterfaceType) ipt;
			ipt = (REF(InterfaceType) &)tpt;
			rpt =ipt->resolve_name(fname,Attribute);
			if (rpt!= 0)
			// this is where we need to handle multiple inheritance.
			{
				offset = rpt->offset;
			}
			return rpt;
		}
		case Sdl_struct:
		{
			REF(StructType) spt;
			spt = (REF(StructType) &)tpt;
			rpt = spt->findMember(fname);
			if (rpt!= 0)
				offset = rpt->offset;
			return rpt;
		}
		case Sdl_union:
		// unions are not fully implemented.
		{
			return 0;
		}
	}
	return 0;
}
	
	
