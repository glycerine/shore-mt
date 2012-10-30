/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef COMMAND_H
#define COMMAND_H

typedef char*	cmd_err_t;

/*
 * Command processing class
 */
class command_base_t {
public:

			command_base_t() {};
    virtual		~command_base_t() {};

    // Commands that get converted to an RPC 
    // All return an error message string that is NULL if success.
    virtual cmd_err_t	commit_transaction() = 0;
    virtual cmd_err_t	abort_transaction() = 0;
    virtual cmd_err_t	clear_grid() = 0;
    virtual cmd_err_t	print_grid(grid_display_t& rows) = 0;
    virtual cmd_err_t	add_item(const char* name, int x, int y) = 0;
    virtual cmd_err_t	remove_item(const char* name) = 0;
    virtual cmd_err_t	move_item(const char* name, int x, int y) = 0;
    virtual cmd_err_t	location_of(const char* name, int& x, int& y) = 0;
    virtual cmd_err_t	spatial_query(const nbox_t& box, spatial_result_t& result) = 0;

    // Command Parsing
    // Print errors to stderr
    // Sets "quit" to true if quit command was found
    void		parse_command(char* line, bool& quit);
};


#endif /* COMMAND_H */
