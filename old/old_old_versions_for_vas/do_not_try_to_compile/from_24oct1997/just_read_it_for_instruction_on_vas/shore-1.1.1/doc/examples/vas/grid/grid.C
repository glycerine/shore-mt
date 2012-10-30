/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#define GRID_C

#include "ShoreConfig.h"
#include <assert.h>
#include "sm_vas.h"
#include "grid_basics.h"
#include "grid.h"

// storage manger to use for all operations
extern ss_m* ssm;


/*
 * Prepare to create a new grid
 */
grid_t::grid_t()
{
}

grid_t::~grid_t()
{
    // make sure we can't reuse this space
    lvid = lvid_t::null;
}

/*
 * Access an already created grid
 */
void
grid_t::init(const lvid_t& _lvid, const grid_info_t& _info)
{
    lvid = _lvid;
    info = _info;
}

rc_t
grid_t::add_item(const char* name, int x, int y)
{
    assert(is_initialized());
    assert(strlen(name) <= MAX_NAME_LEN);
    rc_t rc;

    // create a savepoint, so that if any operation fails
    // we can roll back to this point;
    sm_save_point_t save_point;
    W_DO(ss_m::save_work(save_point));

    // create record for item
    item_t item(name, x, y);
    serial_t item_id;
    do {
	// create the item record
	//
	// note: the use of anonymous vectors since none of the data
	//       to store is scattered in memory
	// note: the ugly parameter comments are used because of a gcc bug
	rc = ssm->create_rec(lvid, info.item_file,	
			 vec_t(),	/* empty record header	*/
			 sizeof(item),	/* length hint		*/
			 vec_t(&item, sizeof(item)), /* body	*/
			 item_id);	/* new rec id		*/
	if (rc) break;

	// add item to index on name
	rc = ssm->create_assoc(lvid, info.name_index,		
			vec_t(item.name, strlen(item.name)), /* key */
			vec_t(&item_id, sizeof(item_id)));   /* element */

	if (rc) break;

	// 
	// add item to Rtree (spatial) index
	// 
	// coord holds the coordinates of the "rectangle" (actually a 
	// point) containing the item
	//
	int coord[4];
	coord[0] = coord[2] = x;
	coord[1] = coord[3] = y;
	nbox_t box(2, coord);
	rc = ssm->create_md_assoc(lvid, info.spatial_index, box,
			vec_t(&item_id, sizeof(item_id)));  // element

    } while (0);

    if (rc) {
	// an error occurred, so rollback to the savepoint
	W_DO(ss_m::rollback_work(save_point));
	return rc;
    }
    return RCOK;
}

rc_t
grid_t::remove_item(const char* name, bool& found)
{
    assert(is_initialized());

    serial_t	id;			// ID of item to remove
    W_DO(get_id(name, id));

    if (id == serial_t::null) {
	found = false;
	return RCOK;
    }

    // create a savepoint, so that if any operation fails
    // we can roll back to this point;
    sm_save_point_t save_point;
    W_DO(ss_m::save_work(save_point));

    rc_t rc;
    do {

	// remove item from index on name
	rc = ssm->destroy_assoc(lvid, info.name_index,
				vec_t(name, strlen(name)),
				vec_t(&id, sizeof(id)));
	if (rc) break;

	//
	// remove from spatial index (first must pin to find coordinates)
	//
	int coord[4];
	{
	    pin_i handle;
	    rc = handle.pin(lvid, id, 0);
	    if (rc) break;
	    const item_t* item = (const item_t*) handle.body();
	    coord[0] = coord[2] = item->x;
	    coord[1] = coord[3] = item->y;
	}
	nbox_t box(2, coord);
	rc = ssm->destroy_md_assoc(lvid, info.spatial_index, box,
				vec_t(&id, sizeof(id)));	/* element */
	if (rc) break;

	// destroy the item
	rc = ssm->destroy_rec(lvid, id);
	if (rc) break;

    } while (0);

    if (rc) {
	// an error occurred, so rollback to the savepoint
	W_DO(ss_m::rollback_work(save_point));
	return rc;
    }

    found = true;
    return RCOK;
}

rc_t
grid_t::move_item(const char* name, int x, int y, bool& found)
{
    assert(is_initialized());

    serial_t	id;			// ID of item to move
    W_DO(get_id(name, id));

    if (id == serial_t::null) {
	found = false;
	return RCOK;
    }

    // pin the item record
    pin_i handle;
    W_DO(handle.pin(lvid, id, 0));
    const item_t& item = *(const item_t*)handle.body();
    assert(strcmp(item.name, name) == 0);

    // 
    // Now we remove the item from the R*tree, update the
    // x,y coordinates, and re-insert it
    //
    // In general, the only way to "update" and index entry
    // is to remove it and re-insert the changed entry.
    //

    // We create a savepoint, so that if any operation fails
    // we can roll back to this point;
    sm_save_point_t save_point;
    W_DO(ss_m::save_work(save_point));

    rc_t rc;
    do {

	//
	// remove from spatial index (first must pin to find coordinates)
	//
	int coord[4];
	coord[0] = coord[2] = item.x;
	coord[1] = coord[3] = item.y;
	nbox_t box(2, coord);
	rc = ssm->destroy_md_assoc(lvid, info.spatial_index, box,
				vec_t(&id, sizeof(id)));	/* element */
	if (rc) break;
  
	//
  	// Now we need to update the item.  Note that it is illegal
	// to "update-in-place", so we create a new item and
	// use the update_rec method
	//

	// init a new item at the new location
	item_t new_item("", x, y);

	// update x,y in the pinned item 
	vec_t new_data(&new_item.x, sizeof(new_item.x) + sizeof(new_item.y));
	rc = handle.update_rec((smsize_t) offsetof(item_t, x), new_data);
	if (rc) break;

	//
	// add item back into the R*tree at new location
	//
	coord[0] = coord[2] = x;
	coord[1] = coord[3] = y;
	rc = ssm->create_md_assoc(lvid, info.spatial_index, nbox_t(2, coord),
			    vec_t(&id, sizeof(id)));	/* element */

	if (rc) break;

    } while (0);

    if (rc) {
	// an error occurred, so rollback to the savepoint
	W_DO(ss_m::rollback_work(save_point));
	return rc;
    }

    found = true;
    return RCOK;
}

rc_t
grid_t::location_of(const char* name, int& x, int& y, bool& found)
{
    assert(is_initialized());

    serial_t	id;			// ID of item to remove
    W_DO(get_id(name, id));

    if (id == serial_t::null) {
	found = false;
	return RCOK;
    }

    // pin the item record to get x,y
    pin_i handle;
    W_DO(handle.pin(lvid, id, 0));

    const item_t& item = *(const item_t*)handle.body();
    assert(strcmp(item.name, name) == 0);

    x = item.x;
    y = item.y;

    found = true;
    return RCOK;
}


rc_t
grid_t::generate_display(grid_display_t& display)
{
    for (int row = 0; row < MAX_GRID_Y; row++) {
	for (int col = 0; col < MAX_GRID_X; col++) {
	    display.rows[row][col] = '+';
	}
    }

    // add first character of each item item name the display
    scan_file_i	scan(lvid, info.item_file);
    pin_i*	handle;	// handle on current record
    bool	eof = false;
    const item_t* item;

    // scan item file and remove all items
    W_DO(scan.next(handle, 0, eof));
    while (!eof)  {
	item = (const item_t*)handle->body();
	display.rows[item->y][item->x] = item->name[0];
	W_DO(scan.next(handle, 0, eof));
    }

    return RCOK;
}


/*
 * This method removes all items and their associated index entries
 */
rc_t
grid_t::clear()
{
    // since we will eventually lock every item, we first obtain
    // an exclusive lock on the file and indexes so that finer
    // granularity locks are not obtained -- thus improving
    // the performance of this method
    W_DO(ss_m::lock(lvid, info.item_file, EX));
    W_DO(ss_m::lock(lvid, info.name_index, EX));
    W_DO(ss_m::lock(lvid, info.spatial_index, EX));

    scan_file_i	scan(lvid, info.item_file);
    pin_i*	handle;	// handle on current record
    bool	eof = false;
    const item_t*	item;
    rc_t	rc;

    // create a savepoint, so that if any operation fails
    // we can roll back to this point;
    sm_save_point_t save_point;
    W_DO(ss_m::save_work(save_point));

    // scan item file and remove all items
    rc = scan.next(handle, 0, eof);
    while (!eof && !rc) {
	item = (const item_t*)handle->body();
	const serial_t& id = handle->serial_no();

	// remove from name index (key==name, element==serial# of item) 
	rc = ss_m::destroy_assoc(lvid, info.name_index,
				vec_t(item->name, strlen(item->name)),
				vec_t(&id, sizeof(serial_t)));
	if (rc) break;
    
	// remove from spatial index
	int coord[4];
	coord[0] = coord[2] = item->x;
	coord[1] = coord[3] = item->y;
	nbox_t box(2, coord);
	rc = ssm->destroy_md_assoc(lvid, info.spatial_index, box,
				vec_t(&id, sizeof(serial_t)));
	if (rc) break;

	// remove item itself
	rc = ss_m::destroy_rec(lvid, id);
	if (rc) break;

	rc = scan.next(handle, 0, eof);
    }
    if (rc) {
	// an error occurred, so rollback to the savepoint
	W_DO(ss_m::rollback_work(save_point));
	return rc;
    }

    return RCOK;
}


rc_t
grid_t::spatial_query(const nbox_t& box, spatial_result_t& result)
{
    assert(is_initialized());

    result.found_cnt = 0;

    serial_t	id;
    smsize_t	id_len = sizeof(id);
    nbox_t	key;
    bool 	eof;
    pin_i 	handle;

    scan_rt_i scan(lvid, info.spatial_index, nbox_t::t_overlap, box);
    W_DO(scan.next(key, &id, id_len, eof));
    while (!eof) {
	assert(id_len == sizeof(id));
	if (result.found_cnt < MAX_SPATIAL_RESULT) {
	    // pin the item record to get x,y
	    W_DO(handle.pin(lvid, id, 0));

	    const item_t& item = *(const item_t*)handle.body();
	    strcpy(result.items[result.found_cnt].name, item.name);
	    result.items[result.found_cnt].x = item.x;
	    result.items[result.found_cnt].y = item.y;
	    assert(item.x == key.bound(0));
	    assert(item.y == key.bound(1));
	}

	result.found_cnt++;
	W_DO(scan.next(key, &id, id_len, eof));
    }
    return RCOK;
}



rc_t
grid_t::get_id(const char* name, serial_t& id)
{
    bool found;

    // find ID (serial#) of item
    // find_assoc will fill &id with the ID.  For safety, we
    // set id_len to sizeof(id) so that no bytes beyond id will
    // be written in case we accidentally put something to large in
    // the index.
    smsize_t	id_len = sizeof(id);  
    W_DO(ssm->find_assoc(lvid, info.name_index,
			vec_t(name, strlen(name)),
			&id, id_len, found));

    if (!found) {
	id = serial_t::null;
    }
    assert(id_len == sizeof(id)); 
    return RCOK;
}
