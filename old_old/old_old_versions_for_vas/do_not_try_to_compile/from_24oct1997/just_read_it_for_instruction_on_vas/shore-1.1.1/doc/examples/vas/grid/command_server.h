/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef COMMAND_SERVER_H
#define COMMAND_SERVER_H

/*
 * Server command processing class
 */
class command_server_t : public command_base_t {
public:
			command_server_t();
			~command_server_t();
   
    // RPC methods
    virtual cmd_err_t	commit_transaction();
    virtual cmd_err_t	abort_transaction();
    virtual cmd_err_t	clear_grid();
    virtual cmd_err_t	print_grid(grid_display_t& rows);
    virtual cmd_err_t	add_item(const char* name, int x, int y);
    virtual cmd_err_t	remove_item(const char* name);
    virtual cmd_err_t	move_item(const char* name, int x, int y);
    virtual cmd_err_t	location_of(const char* name, int& x, int& y);
    virtual cmd_err_t   spatial_query(const nbox_t& box, spatial_result_t& result);

    static lvid_t	lvid;		// volume containing grid
private:
    static const char*	grid_name;	// root name of grid 

    rc_t		init();

    grid_t		grid;		// grid to serve requests for

    // These are used to generate error replies
    ostrstream		err_strstream;	
    char		_err_space[MAX_ERR_MSG_LEN];
};

#endif /* COMMAND_SERVER_H */
