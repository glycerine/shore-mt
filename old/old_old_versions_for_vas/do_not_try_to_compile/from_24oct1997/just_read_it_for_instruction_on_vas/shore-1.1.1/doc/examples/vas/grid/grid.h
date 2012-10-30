/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef GRID_H
#define GRID_H

/*
 * Class managing grid and items on the grid 
 */
class grid_t {
public:
    struct grid_info_t {
	serial_t item_file;	// ID of file of item records
	serial_t name_index;	// B+tree mapping names to item records
	serial_t spatial_index;	// R*tree mapping points to items
    };

    		grid_t();

    		~grid_t();
		// Access an already created grid
    void	init(const lvid_t& lvid, const grid_info_t& info);
    rc_t	add_item(const char* name, int x, int y);
    rc_t	remove_item(const char* name, bool& found);
    rc_t	move_item(const char* name, int x, int y, bool& found);
    rc_t	location_of(const char* name, int& x, int& y, bool& found);
    rc_t	generate_display(grid_display_t& display);
    rc_t	spatial_query(const nbox_t& box, spatial_result_t& result);
    rc_t	clear();	// remove all items from grid

    bool	is_initialized() const {return lvid != lvid_t::null;}
private:
    lvid_t	lvid;		// ID of volume containing grid;
    grid_info_t info;

    // get the ID of the item "name", return serial_t::null if
    // not found.
    rc_t	get_id(const char* name, serial_t& id);

};

#endif /* GRID_H */
