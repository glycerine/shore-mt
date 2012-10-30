/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#include <ShoreApp.h>
#include <ObjCache.h>
#include <sdl_index.h>
#include "errlog.h"
#include "debug.h"
#include "svas_base.h"


// calls to set sdl runtime context
int 
push_sdlrt(vec_t * vpt,int vin);
int 
pop_sdlrt();

shrc sdl_index_base::init(IndexKind kind) const
{
	W_DO(Shore::addIndex(index_id(),kind));
	return RCOK;
}

int 
sdl_index_base::my_vindex() const 
{ return OC_ACC(vt).lookup(index_id().obj.lvid);}

OCRef 
sdl_index_base::my_OCRef() const
// update the max_keysize field with a new value; we must
// mark the parent object containing this index as updated.
{
	OCRef my_ref;
	LOID t_loid;
	t_loid.id = index_id().obj;
	my_ref.assign(t_loid);
	return my_ref;
}


void
sdl_index_base::update_max_keysize(size_t newlen) const
{
	my_OCRef().make_writable(); // mark object containing "this" dirty
	// cast out constness & update field.
	((sdl_index_base *)this)->max_keysize = newlen;
}

void
sdl_index_base::update_max_valsize(size_t newlen) const
{
	my_OCRef().make_writable(); // mark object containing "this" dirty
	// cast out constness & update field.
	((sdl_index_base *)this)->max_valsize = newlen;
}



shrc sdl_index_base::insert(void * kpt, size_t klen, void *  elt,size_t elen) const
{
	// construct the 2 vecs
	vec_t key_vec;
	vec_t val_vec;
	key_vec.put(kpt,klen);
	val_vec.put(elt,elen);

	W_DO(Shore::insertIndexElem(index_id(),key_vec,val_vec)) ;
	return RCOK;
}

shrc sdl_index_base::insert(const IndValBase &kval, const IndValBase &eval)const
{
	// construct the 2 vecs
	vec_t key_vec;
	vec_t val_vec;
	kval.set_output_vec(&key_vec,my_vindex());
	eval.set_output_vec(&val_vec,my_vindex());
	W_DO(Shore::insertIndexElem(index_id(),key_vec,val_vec)) ;
	if (key_vec.size() > max_keysize)
		update_max_keysize(key_vec.size());
	if (val_vec.size() > max_valsize)
		update_max_valsize(val_vec.size());
	return RCOK;
}

shrc 
sdl_index_base::remove(void *kpt, size_t klen , int &nrm) const
{
	vec_t key_vec;
	key_vec.put(kpt,klen);
	W_DO(Shore::removeIndexElem(index_id(),key_vec,&nrm)) ;
	return RCOK;
}


shrc 
sdl_index_base::remove(const IndValBase & kval, int &nrm) const
{
	vec_t key_vec;
	kval.set_output_vec(&key_vec,my_vindex());
	W_DO(Shore::removeIndexElem(index_id(),key_vec,&nrm)) ;
	return RCOK;
}

shrc 
sdl_index_base::remove(void * kpt, size_t klen, void *  elt,size_t elen) const
{
	// construct the 2 vecs
	vec_t key_vec;
	vec_t val_vec;
	key_vec.put(kpt,klen);
	val_vec.put(elt,elen);

	W_DO(Shore::removeIndexElem(index_id(),key_vec,val_vec)) ;
	return RCOK;
}

shrc 
sdl_index_base::remove_ref(void * kpt, size_t klen, OCRef  elt) const
{
	// construct the 2 vecs
	vec_t key_vec;
	vec_t val_vec;
	// should probably do some checking...
	W_DO(elt.unswizzle(my_vindex()));
	key_vec.put(kpt,klen);
	val_vec.put(&elt,sizeof(elt));

	W_DO(Shore::removeIndexElem(index_id(),key_vec,val_vec)) ;
	return RCOK;
}

	
shrc 
sdl_index_base::find(const IndValBase &kval, IndValBase &eval, ObjectSize &ret_len,  bool &found) const
{
	// construct the 2 vecs
	vec_t key_vec;
	vec_t val_vec;
	char * elt_buf = 0;
	if (max_keysize == 0 && max_valsize ==0)
	// empty index??
	{
		found = false;
		return RCOK;
	}
	kval.set_output_vec(&key_vec,my_vindex());
	eval.set_input_vec(&val_vec);
	W_DO(Shore::findIndexElem(index_id(),key_vec,val_vec,&ret_len,&found)) ;
	return RCOK;
}

shrc 
sdl_index_base::find(void *kpt, size_t klen, void * elt, size_t elen, bool &found) const
{
	// construct the 2 vecs
	vec_t key_vec;
	vec_t val_vec;
	ObjectSize ret_len;
	key_vec.put(kpt,klen);
	val_vec.put(elt,elen);

	W_DO(Shore::findIndexElem(index_id(),key_vec,val_vec,&ret_len,&found)) ;
	return RCOK;
}

	
// remove version for swizzled/vsized key/vals
// general purpose base classes for apply fct based swizzled/vsized
// keys and values.  We may be able to special case strings out of
// this too, via explicit tests for the string apply fct??


shrc
sdl_index_base::remove(const IndValBase & kval, const IndValBase & eval) const
{
	// construct the 2 vecs
	vec_t key_vec;
	vec_t val_vec;
	kval.set_output_vec(&key_vec,my_vindex());
	eval.set_output_vec(&val_vec,my_vindex());
	W_DO(Shore::removeIndexElem(index_id(),key_vec,val_vec)) ;
	return RCOK;
}


shrc
sdl_index_base::
openScan( CompareOp o1,const IndValBase & kval, CompareOp o2, const IndValBase & kv2,
	Cookie & ck) const
{
	// construct the 2 vecs
	vec_t key1_vec;
	vec_t key2_vec;
	kval.set_output_vec(&key1_vec,my_vindex());
	kv2.set_output_vec(&key2_vec,my_vindex());
	W_DO(Shore::openIndexScan(index_id(),o1,key1_vec,o2,key2_vec,ck));
	return RCOK;
}


// for unknown reasons, everything ends up with noappIndVal<sdl_string>.
template class noappIndVal<sdl_string>;
template class noappIndVal<long>;
template class noappIndVal<short>;
template class noappIndVal<char>;
template class noappIndVal<bool>;
template class noappIndVal<unsigned long>;  
template class noappIndVal<unsigned short>;
template class noappIndVal<unsigned char>;
template class noappIndVal<float>;        
template class noappIndVal<double>;


