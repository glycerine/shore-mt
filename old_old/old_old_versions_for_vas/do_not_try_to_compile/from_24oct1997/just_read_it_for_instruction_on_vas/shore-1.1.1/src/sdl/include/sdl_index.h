/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef _sdl_index_h
#define _sdl_index_h 1

typedef void (*apply_fpt)(void *p, HeapOps op);
// calls to set sdl runtime context
int 
push_sdlrt(vec_t * vpt,int vin);
int 
pop_sdlrt();
void set_sdl_heappt(void * newheap);

class IndValBase 
// encapulate things used to pass indicies into shore.
// this should be used only as base class of an instance of
// IndValBase defined below, e.g. an abstract base class.
{
public:
	void * space_pt ; // for input vector only.
	int ilen;
	
	virtual void set_output_vec(vec_t *vpt, int) const = 0;
	// virtual ~IndValBase()= 0; // not strictly necessary.
	// ignore this if we don't get warnings. 
	IndValBase() { ilen = 0; space_pt = 0;};
	void set_input_vec(vec_t *vpt) 
	{
		// assert ((space_pt != 0) &&(ilen != 0));
		// if nothing has ever been insterted, ilen & space_pt
		// may be 0.
		if (ilen==0) // may cause problems, so buffer 1 byte.
		{	ilen = 1; space_pt = new char; }
		vpt->put(space_pt,ilen);
	}
	//  we used to have a separate destructor, but it blows.
	// but it blows anyway...
	//~IndValBase() { if (space_pt && (space_pt<this || space_pt > this+2)) delete space_pt;};
};

class sdl_index_base
{
  public:
	// IndexId index_id;
	// since we don't #ifdef ctors anymore, do a space hack for this.
	char index_id_space[sizeof(IndexId)];
	IndexId & index_id() const { return (IndexId &)index_id_space[0]; }
	// we need to save the maximum key & value stored, so that we can size
	// retrieves appropriately.
	smsize_t max_keysize;
	smsize_t max_valsize;
	shrc init(IndexKind kind) const;
	shrc insert(void * kpt, size_t klen, void *  elt,size_t elen) const;
	shrc insert(const IndValBase &, const IndValBase & ) const;
	shrc remove(void *kpt, size_t klen , int &nrm) const;
	shrc remove(const IndValBase & , int &nrm) const;
	shrc remove(void * kpt, size_t klen, void *  elt,size_t elen) const;
	shrc remove(const IndValBase&, const IndValBase & ) const;
	shrc find(void *kpt, size_t klen,void * elt, size_t elen,bool &found) const;
	shrc find(const IndValBase &, IndValBase &,ObjectSize &,bool &found) const;
	// special case for ref elts- pass as OCRef *, unswizzle on
	// insertion, swizzle on retrieval.
	shrc remove_ref(void * kpt, size_t klen, OCRef  elt) const;
	shrc insert_ref(void * kpt, size_t klen, OCRef  elt) const;
	shrc find_ref(void *kpt, size_t klen, OCRef &elt, bool & found) const;
	// general purpose routines with apply fct. parms, to handle swizzled/
	// variable size keys & values.
	shrc remove_app(void * kpt, size_t klen, apply_fpt k_apply) const;
	shrc remove_app(void * kpt, size_t klen, apply_fpt k_apply,
		void *eltpt, size_t elen, apply_fpt e_apply) const;
	shrc find_app(void * kpt, size_t klen, apply_fpt k_apply,
		void *eltpt, size_t elen, apply_fpt e_apply, bool & found) const;
	shrc insert_app(void * kpt, size_t klen, apply_fpt k_apply,
		void *eltpt, size_t elen, apply_fpt e_apply) const;

	shrc openScan_app( CompareOp o1,void * k1pt, size_t k1len,
			CompareOp o2,void * k2pt, size_t k2len, apply_fpt k2_apply,
			Cookie & ck) const;
	shrc openScan( CompareOp o1,const IndValBase &,
			CompareOp o2, const IndValBase &,
			Cookie & ck) const;
	shrc nextScan_app(Cookie &ck, void *k, ObjectSize &kl, apply_fpt k_apply,
			void *val, ObjectSize &vl, apply_fpt v_apply, bool & eof) const;


	// scan things for no sw/ref.
	shrc openScan(CompareOp o1,void *k1, size_t kl1, 
			CompareOp o2, void*k2,size_t kl2,
			Cookie & ck) const
	{   
		vec_t kv1, kv2;
		kv1.put(k1,kl1);
		kv2.put(k2,kl2);
		W_DO(Shore::openIndexScan(index_id(),o1,kv1,o2,kv2,ck)); 
		return RCOK;
	}
	shrc nextScanVal(Cookie &ck, void *k, ObjectSize &kl, void *val, ObjectSize &vl,bool & eof) const
	{	vec_t kv; vec_t vv;
		kv.put(k,kl); vv.put(val,vl);
		W_DO(Shore::nextIndexScan(ck,kv,kl,vv,vl,eof)); 
		return RCOK; 
	}
	shrc nextScanRef(Cookie &ck, void *k, size_t &kl, 
				OCRef &elt, bool &eof) const;
	shrc closeScan(Cookie &ck, void *) const
	{   
		W_DO(Shore::closeIndexScan(ck)); 
		return RCOK;
	}
	void __apply(HeapOps op);
	int my_vindex() const;
private:
	void update_max_keysize(size_t) const;
	void update_max_valsize(size_t) const;
	OCRef my_OCRef() const;
};

 



template <class v>
class noappIndVal : public IndValBase
// wrapper class for various paramemter values used by indices,
// used to do any conversion needed in translating from application/mem
// format to/from the format which the underlying shore index expects.
// Note: the noappIndVal is used for data types that don't require
// swizzling.; the IndVal template is used for data types that do.
{
	// note : space_pt inherited from IndValBase.
protected:
	v val_copy;
public:

	void set_output_vec(vec_t *vpt, int /* vin */)  const { 
		vpt->put(&val_copy,sizeof(v));
	};
	// by construction, this doesn't have to be virtual ( maybe)
	void prepare_input(int /* vin */, size_t /* vin */) {} // nothing by default.
	v & value() const { return space_pt? *(v *)space_pt : (v &)val_copy; }
	noappIndVal(const v &val) { val_copy = val;}; 
	//used for input parameters.
	noappIndVal( void* space= 0, int s= sizeof(v)  )  
	{ 
		if (space)
			space_pt = space;
		else if (s == sizeof(v))
			space_pt = &val_copy;
		else if (s)
			space_pt  = new char[s];
		else
			space_pt = 0;
		ilen = s;
	}
			
	// this doesn't really need to be virtual except to suppress compiler
	// warnings.  ho,ho.

	// as a matter of fact, it causes gcc 2.7.2 to fail, so we get rid
	// of destructors all together; this can lead to some garbage
	// problems (i.e. memory leaks) which need to be addressed by
	// other mechanisms.
	// virtual  ~noappIndVal() { if (space_pt && space_pt!= &val_copy) delete space_pt;};
};

// version of IndVal used for things with apply fcts.
// this will normally be used to override the default 
// definition of IndVal<t> via 
// class IndVal<t> : appIndVal<t>
template <class v>
class IndVal : public noappIndVal<v>
// wrapper class for various paramemter values used by indices,
// used to do any conversion needed in translating from application/mem
// format to/from the format which the underlying shore index expects.
{
public:
	void set_output_vec( vec_t *vpt, int vin) const {
	 	push_sdlrt(vpt,vin);
		vpt->put(&value(),sizeof(v));
		value().__apply(PrepareForDisk); 
		pop_sdlrt(); 
	};
	void prepare_input(int vin, size_t len) 
	{
		len=len; //defeat bogus -Wall warning.
	 	push_sdlrt(0,vin);
		set_sdl_heappt(& value() +1); // set heap at end of value?
		value().__apply(PrepareForMem); 
		pop_sdlrt(); 
	};
	IndVal(const v &val): noappIndVal<v>(val){}; 
	//used for input parameters.
	IndVal( void* space= 0, int s= sizeof(v)  ) 
		:noappIndVal<v>(  space,  s  )  {}
};

// define a macro to override the template definition of IndVal for things
// that should really use noappIndVal.  This is currently used for primitive
// types (long float etc); it should for anything without an __apply member.
// The macro is essentially
// typedef noappIndVal<T> IndVal<T>
// but that notation doesn't quite do it.
#define OVERRIDE_INDVAL(T)	\
class IndVal<T> : public noappIndVal<T> {				\
public:									\
	IndVal(const T & v):noappIndVal<T>(v) {};			\
	IndVal( void* space= 0, int s= sizeof(T)  ) 			\
		:noappIndVal<T>(  space,  s  )  {}			\
};
OVERRIDE_INDVAL(long)
OVERRIDE_INDVAL(short)
OVERRIDE_INDVAL(char)
OVERRIDE_INDVAL(bool)
OVERRIDE_INDVAL(unsigned long)
OVERRIDE_INDVAL(unsigned short)
OVERRIDE_INDVAL(unsigned char)
OVERRIDE_INDVAL(float)
OVERRIDE_INDVAL(double)

// specialized version of IndVal for strings; this translates sdl_string
// to a char * string for use within the sm indices.
class IndVal<sdl_string> : public noappIndVal<sdl_string>
{
public:
	void set_output_vec( vec_t *vpt, int /* vin */ ) const
	{
		vpt->put(value().string(),value().strlen());
	}
	void prepare_input(int /* vin */ , size_t len) { 
		// make sure it's null terminated...
		((char *)space_pt)[len]=0;value() = (char *)space_pt; 
	};
	IndVal(const sdl_string &v): noappIndVal<sdl_string>(v) {};
	IndVal( void* space= 0, int s= 0  ) : noappIndVal<sdl_string>(sdl_string(0))
	{
		// add 1 to space to allow for null termination.
		if (!space) space = new char[s+1];
		space_pt = space;
		ilen = s;
	}
	sdl_string & value() const { return (sdl_string &)val_copy; } // always; space_pt is different.
};


		
	
// Finally, the Index template used to define index attributes.
template <class Key, class Val>
class Index	: public sdl_index_base 
{

  public:
  // make some typedefs to export the (actual) parms
  // these are only needed for the backward-compatible index_iter template.
	typedef Key KeyType;
	typedef Val ValType;
	shrc insert(const Key &key,const Val &elt) const
	{
		IndVal<Key> kv(key);
		IndVal<Val> ev(elt);
		// return sdl_index_base::insert((const IndVal<Key>)(key), (const IndVal<Val>)(elt));
		return sdl_index_base::insert((IndValBase &)kv,(IndValBase &)ev);
	}
	shrc remove(const Key &key, int &nrm) const
	{
		IndVal<Key> kv(key);
		// return sdl_index_base::remove(IndVal<Key>(key),nrm);
		return sdl_index_base::remove((IndValBase &)kv,nrm);
	}
	shrc remove(const Key &key,const Val &elt) const
	{
		IndVal<Key> kv(key);
		IndVal<Val> ev(elt);
		// return sdl_index_base::remove(IndVal<Key>(key), IndVal<Val>(elt));
		return sdl_index_base::remove((IndValBase &)kv,(IndValBase &)ev);
	}
	shrc find(Key key, Val & elt,bool &found) const
	{
		IndVal<Key> kv(key);
		ObjectSize rl;
		IndVal<Val> tmp(0,max_valsize); // uninitalized - allocation forced.
		// W_DO( sdl_index_base::find(IndVal<Key>(key), tmp,rl,found));
		W_DO( sdl_index_base::find((IndValBase &)kv, (IndValBase &)tmp,rl,found));
		if (found)
		{
			tmp.prepare_input(my_vindex(),rl);
			elt = tmp.value();
		}
		return RCOK;
	}
	shrc openScan(CompareOp o1, const Key &k1, CompareOp o2, const Key & k2, Cookie &rck) const
	{	
		IndVal<Key> kv1(k1);
		IndVal<Key> kv2(k2);
		return sdl_index_base:: openScan(o1,(IndValBase &)kv1, o2,(IndValBase &)kv2, rck);
		// return sdl_index_base:: openScan(o1,IndVal<Key>(k1), o2,IndVal<Key>(k2), rck);
	}
};	

// now define scan iteration class parameterized by the same types
// as the index over which the scan is to take place.
template <class Key, class Val>
class IndexScanIter
{
	CompareOp lcond;
	CompareOp ucond;
	Cookie sck;
	//const ind_t * ipt; // possible pinning problem? yes.
	Index<Key,Val>   ipt; // possible pinning problem? yes.
	// we resolve it by copying the index value. 
	
	int did_init;
	char * key_buf;
	char * elt_buf;
	vec_t key_vec;
	vec_t val_vec;
	IndVal<Key> kval; //bletch
	IndVal<Val>	eval;
	// note: it is essential that the cur_key and cur_val ref fiels
	// follow the kval and eval fields because of order of constructor
	// things.  the order specified in the constructor for this
	// class doesn't matter.
public:
	bool eof;
	Key &cur_key;
	Val &cur_val;
	Key lb;
	Key ub;
private:
	void set_bufs() 
	{

		kval.set_input_vec(&key_vec);
		eval.set_input_vec(&val_vec);
	}
public:
		
	// this is pretty disgusting: initialize cur_key and cur_val
	// refs with pointers to new'd space.
	IndexScanIter(const Index<Key,Val> idx):
		kval(0,idx.max_keysize),
		eval(0,idx.max_valsize),
		cur_key(kval.value()),
		cur_val(eval.value())
	{	
		key_buf = 0;
		elt_buf = 0;
		ipt = idx; did_init = 0;
		lcond = geNegInf;   ucond = lePosInf;
		eof = false;
		set_bufs();
	}
	IndexScanIter(const Index<Key,Val> &idx,Key l, Key u) :
		kval(0,idx.max_keysize),
		eval(0,idx.max_valsize),
		cur_key(kval.value()),
		cur_val(eval.value())
	{       //Diid = idx.index_id; 
		key_buf = 0;
		elt_buf = 0;
		ipt = idx;
		lb = l; ub = u;
		lcond = geOp;	ucond = leOp; did_init = 0;
		eof = false;
		set_bufs();
	}
	void SetLowerBound(Key b) {lb = b;}
	void SetUpperBound(Key b) {ub = b;}
	//old names.
	void SetLB(Key b) {lb = b;}
	void SetUB(Key b) {ub = b;}
	void SetLowerCond(CompareOp o) { lcond = o; };
	void SetUpperCond(CompareOp o) { ucond = o; };

	shrc next() { 
		ObjectSize kl, vl;
		if (!did_init) {
			W_DO(ipt.openScan(lcond,lb,
				ucond,ub, sck));
			did_init=1;
		}
		// vectors have been set...
		W_DO(Shore::nextIndexScan(sck,key_vec, kl,val_vec,vl,eof));
		if (!eof) {
			kval.prepare_input(ipt.my_vindex(),kl);
			eval.prepare_input(ipt.my_vindex(),vl);
		}
		return RCOK;
	}
	shrc close()	{ W_DO(ipt.closeScan(sck,&cur_key)); return RCOK; }
	~IndexScanIter() { 
		if (key_buf) delete key_buf;
		if (elt_buf) delete elt_buf;
		W_IGNORE(ipt.closeScan(sck,&cur_key));
	}
};

// backward complatible index_iter parameterized by index type.

template <class ind_t>
class index_iter : public IndexScanIter<ind_t::KeyType,ind_t::ValType>
{
public:
	index_iter(const Index<ind_t::KeyType,ind_t::ValType> &idx) :
		IndexScanIter<ind_t::KeyType,ind_t::ValType>(idx) {}
	index_iter(const Index<ind_t::KeyType,ind_t::ValType> &idx,
		ind_t::KeyType l, ind_t::KeyType u) :
		IndexScanIter<ind_t::KeyType,ind_t::ValType>(idx, l, u) {}
// nothing to add.
};
#endif
