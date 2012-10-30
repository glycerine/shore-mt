/* --------------------------------------------------------------- */
/* -- Copyright (c) 1996 Computer Sciences Department,          -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#include <stdlib.h>
#include <stdio.h>
#include <ostream.h>
#include <stdlib.h>
#include <assert.h>

#include <symbol.h>
#include <types.h>
#include <tree.h>

#include <oql_context.h>

int ProcessUtility(Ql_tree_node* root, oqlContext& context)
{
   oql_rc_t status = OQL_OK;
   const char* name = 0;

   switch(root->_type)
   {
    case create_db_n:
    case drop_db_n:
    case open_db_n:
    case drop_extent_n:
      // assert(root->_kids[0]->_type == id_n);
      name = root->_kids[0]->_id;
      break;
    case create_index_n:
    case create_clustered_index_n:
    case drop_index_n:
      errstream() << "Index operations not yet implemented..." << endl;
      return paraQ_ERROR;
    case close_db_n:
      break;
    default:
      cerr << "Unknown case in switch in function ProcessUtility" << endl;
      para_check(0);
   }
#ifndef STAND_ALONE
   switch(root->_type)
   {
   case create_db_n:
      status = context.create_db(name); 
      break;
   case drop_db_n:
      status = context.drop_db(name); 
      break;
   case open_db_n:
      status = context.open_db(name); 
      break;
   case close_db_n:
      status = context.close_db(); 
      break;
   case drop_extent_n:
      status = context.drop_extent(name); 
      break;
   }
#endif STAND_ALONE
   if (status != OQL_OK)
   {
      errstream() << "Error occurred in processing this utility."
	          << "Dont know [and couldnt care less]"
                  << " where the error is...\n";
      return paraQ_ERROR;
   }
   return paraNOERROR;
}

