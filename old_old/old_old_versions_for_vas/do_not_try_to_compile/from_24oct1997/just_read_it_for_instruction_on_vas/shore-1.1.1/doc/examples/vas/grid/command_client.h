/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef COMMAND_CLIENT_H
#define COMMAND_CLIENT_H

/*
 * Client command processing class
 */
class command_client_t : public command_base_t {
public:
			command_client_t(CLIENT*);
    
    virtual cmd_err_t	commit_transaction();
    virtual cmd_err_t	abort_transaction();
    virtual cmd_err_t	clear_grid();
    virtual cmd_err_t	print_grid(grid_display_t& rows);
    virtual cmd_err_t	add_item(const char* name, int x, int y);
    virtual cmd_err_t	remove_item(const char* name);
    virtual cmd_err_t	move_item(const char* name, int x, int y);
    virtual cmd_err_t	location_of(const char* name, int& x, int& y);
    virtual cmd_err_t   spatial_query(const nbox_t& box, spatial_result_t& result);

private:
    CLIENT*		cl;
};

#endif /* COMMAND_CLIENT_H */
