/* --------------------------------------------------------------- */
/* -- Copyright (c) 1996 Computer Sciences Department,          -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#include <ostream.h>
#include <stdio.h>
#include <tree.h>
#include <malloc.h>
#include <assert.h>

#include <lalloc.h>

Ql_tree_node** alloc_kids(int sz)
{
#ifdef USE_ALLOCATORS
   return (Ql_tree_node **)(DefaultAllocator()->
			    alloc(sz * sizeof(Ql_tree_node *)));
#else  USE_ALLOCATORS
   return new Ql_tree_node *[sz];
#endif USE_ALLOCATORS
}

#ifdef USE_ALLOCATORS
void* Ql_tree_node::operator new(long sz)
{
   return DefaultAllocator()->alloc(sz);
}
#endif USE_ALLOCATORS


void Ql_tree_node::init(NodeType type, int kids)
{
   int num;
   _type = type;

   _nkids = kids;
   _size = kids ? kids + 2 : 0;
 
   if (kids) {
      _kids = alloc_kids(_size);
      for (int i = 0; i < _size; i++)
	 _kids[i] = 0;
   }
   else
      _kids = (Ql_tree_node **)0;
}

//
// Murali.change 3/31
// I'm trying to change flex& bison to be re-entrant. It would make 
// things easier for me, if the node would allocate storage for itself
// I see no reason for the parser to actually handle such allocation
//

// Ql_tree_node::Ql_tree_node(NodeType type, char *id) : _id(id, true)
Ql_tree_node::Ql_tree_node(NodeType type, const char *id) 
    : _id(id) 
{
   init(type, 0);
}

Ql_tree_node::Ql_tree_node(NodeType type, int nkids)
{
   init(type, nkids);
}

Ql_tree_node::Ql_tree_node(NodeType type, int nkids,
			   Ql_tree_node *one,
			   Ql_tree_node *two,
			   Ql_tree_node *three,
			   Ql_tree_node *four)
{
   init(type, nkids);
   switch (nkids) {
   case 4:  _kids[3] = four;
   case 3:  _kids[2] = three;
   case 2:  _kids[1] = two;
   case 1:  _kids[0] = one;
   }
}

Ql_tree_node::~Ql_tree_node()
{
   int	i;

   /* Free kids first ; they can have reference to our info */
/** No freeing **/
/**
   for (i = 0; i < _nkids; i++) {
      delete _kids[i];
      _kids[i] = (Ql_tree_node *)0;
   }
   // Free the pointers to the kids 
   if (_nkids)
      delete _kids;
**/
}

Ql_tree_node *Ql_tree_node::add(Ql_tree_node *son)
{
   Ql_tree_node **old, **next;
   int	i;

   if (_nkids < _size - 1)
   {
      _kids[_nkids++] = son;
      return this;
   }
   _size += 5;
   old = _kids;
   next = alloc_kids(_size);

   for (i = 0; i < _nkids; i++)
      next[i] = old[i];
   next[_nkids++] = son;
   _kids = next;

#ifndef USE_ALLOCATORS
   delete old;
#endif  USE_ALLOCATORS

   return this;
}

#if 0
/* Steal the mallocated string from the node */ 
char *Ql_tree_node::steal()
{
#if 0
   char	*id = _id;
   _id = (char *)0;
   return id;
#else
   /* Can't steal the id; some parts of the tree need to be
      converted MULTIPLE times ! */
   if (!_id || !_id[0])
      errstream().form("Warning: stealing an empty id from tree %#lx\n",
		(long )this);
   return strdup(_id);
#endif
}
#endif

static char *indent(int level)
{
   static char buf[1024];
   static char spaces[] = "    ";
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

ostream &Ql_tree_node::print(ostream &s, int level)
{
   int	i;

   if (_nkids == 0) {
      s << form("[%s], [id = %s]\n", node_name(_type),  _id.str());
      return s;
   }

   s << form("[%s] ", node_name(_type)) << form(" %d kids:\n", _nkids);

   for (i = 0; i < _nkids; i++) {
      s << indent(level+1) <<  form("[%d] ", i);
      _kids[i]->print(s, level+1);
   }
   return s;
}

ostream &operator<<(ostream &s, Ql_tree_node *node)
{
   return node->print(s);
}

ostream &operator<<(ostream &s, Ql_tree_node &node)
{
   return node.print(s);
}

#include "node_names.h"

char *node_name(NodeType type)
{
   static char errbuf[128];
   if (type < 0 || type >= num_node_names) {
      sprintf(errbuf, "<unknown %d>", type);
      return errbuf;
   }
   return node_names[type];
}
