/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 * This file implements the main() code for the grid client program
 */

#include "ShoreConfig.h"
#include <stream.h>
#include <string.h>
#include <strstream.h>
#include <ctype.h>
#include <rpc/rpc.h>
// include stuff needed for SM applications (clients)
// use this rather than sm_vas.h since it's small and all that
// is necessary for this file
#include "grid_basics.h"
#include "sm_app.h"
#include "nbox.h"
#include "grid.h"
#include "command.h"

enum command_token_t {
    commit_cmd,
    abort_cmd,
    clear_cmd,
    print_cmd,
    add_cmd,
    remove_cmd,
    move_cmd,
    locate_cmd,
    spatial_cmd,
    quit_cmd,
    help_cmd
};

struct command_description_t {
    command_token_t	token;
    int		  	param_cnt;	// number of parameters
    char* 		name;		// string name of command
    char* 		parameters;	// parameter list
    char* 		description;	// command description
};

static command_description_t descriptions[] = {
    {commit_cmd, 0, "commit", "", 	"commit transaction and start another one"},
    {abort_cmd,  0, "abort",  "", 	"abort transaction and start another one"},
    {clear_cmd,  0, "clear",  "", 	"clear grid"},
    {print_cmd,  0, "print",  "", 	"print grid"},
    {add_cmd,    3, "add",    "name x y", "add new item <name> at <x,y>"},
    {remove_cmd, 1, "remove", "name", 	"remove item <name>"},
    {move_cmd, 3, "move", "name x y", 	"move item <name> to location <x,y>"},
    {locate_cmd, 1, "locate", "name", 	"print location of item <name>"},
    {spatial_cmd, 4, "spatial", "x_lo y_lo x_hi y_hi", 	"print count of items in rectangle and list first few items"},
    {quit_cmd,   0, "quit",   "", 	"quit and exit program (aborts current transaction)"},
    {help_cmd,   0, "help",   "", 	"prints this message"}
};

// number of commands
static command_cnt = sizeof(descriptions)/sizeof(command_description_t);

static void
print_commands() 
{
    cerr << "Valid commands are: \n"<< endl;
    const command_description_t* cmd;
    for (cmd = descriptions; cmd != descriptions+command_cnt; cmd++) {
	cerr << "    " << cmd->name << " " << cmd->parameters << endl;
	cerr << "        " << cmd->description << endl;
    }
    cerr << "\n    Comments begin with a '#' and continue until the end of the line." << endl;
}

static void
print_usage(const command_description_t* cmd) 
{
    cerr << "Usage: "<< cmd->name << " " << cmd->parameters << endl;
}

void
command_base_t::parse_command(char* line, bool& quit)
{
    istrstream	s(line);

    const	max_params = 5;
    char*	params[max_params];
    int		param_cnt = 0;
    int		i;

    // find all parameters in the line (parameters begin
    // with non-white space) and end each parameter with \0
    bool in_param = false;	// not current in a parameter
    for (i = 0; line[i] != '\0'; i++) {
	if (in_param) {
	    if (isspace(line[i])) {
		// end of parameter
		line[i] = '\0';
		in_param = false;
	    }
	} else {
	    if (line[i] == '#') {
		// rest of line is comment
		break;
	    }

	    if (!isspace(line[i])) {
		// beginning of parameter
		if (param_cnt == max_params) {
		    cerr << "Error: too many parameters." << endl;
		    return;
		}
		params[param_cnt] = line+i;
		param_cnt++;
		in_param = true;
	    }
	}
    }

    if (param_cnt == 0) {
	// blank line
	return;
    }

    // Search for command in command list
    command_description_t* cmd;
    for (cmd = descriptions; cmd != descriptions+command_cnt; cmd++) {
	// command is recognized with just first 2 characters
	if (strncmp(params[0], cmd->name, 2) == 0) {
	    break;
	}
    }
    if (cmd == descriptions+command_cnt) {
	// command not found
	cerr << "Error: unkown command " << params[0] << endl;
	print_commands();
    } else if (cmd->param_cnt != param_cnt-1) {
	// wrong number of parameters
	cerr << "Error: wrong number of parameters for " << cmd->name << endl;
	print_usage(cmd);
    } else {

	// call proper RPC for the command

	int 	x;
	int 	y;
	char* 	name;
	quit = false;

	cmd_err_t err = 0;
	switch(cmd->token) {
	case commit_cmd:
	    err = commit_transaction();
	    if (!err) {
		cout << "transaction is committed -- new one started" << endl;
	    }
	    break;
	case abort_cmd:
	    err = abort_transaction();
	    if (!err) {
		cout << "transaction is rolled back -- new one started" << endl;
	    }
	    break;
	case clear_cmd:
	    err = clear_grid();
	    if (!err) {
		cout << "grid has been cleared" << endl;
	    }
	    break;
	case print_cmd:
	    grid_display_t display;
	    err = print_grid(display);
	    if (err) break;

	    // print header line
	    cout << "\n    ";
	    for (int col = 0; col < MAX_GRID_X; col++) {
		if (col%10 == 0) {
		    cout << '.';
		} else {
		    cout << col%10;
		}
	    } 
	    cout << endl;
	    // print rows
	    for (int row = 0; row < MAX_GRID_Y; row++) {
		cout << form("%.3i", row) << " " ;
		for (int col = 0; col < MAX_GRID_X; col++) {
		    cout << display.rows[row][col];
		}
		cout << endl;
	    }
	    break;
	case add_cmd:
	    name = params[1];
	    x = strtol(params[2], 0, 0);
	    y = strtol(params[3], 0, 0);
	    if (x < 0 || x >= MAX_GRID_X) {
		cerr << "Error: x parameter must be >=0 and < " << MAX_GRID_X << endl;
		break;
            }
	    if (y < 0 || y >= MAX_GRID_Y) {
		cerr << "Error: y parameter must be >=0 and < " << MAX_GRID_Y << endl;
		break;
            }
   	    err = add_item(name, x, y);
	    if (!err) {
		cout << "new item " << name << " has been added" << endl;
	    }
	    break;
	case remove_cmd:
	    name = params[1];
   	    err = remove_item(name);
	    if (!err) {
		cout << "item " << name << " has been removed" << endl;
	    }
	    break;
	case move_cmd:
	    name = params[1];
	    x = strtol(params[2], 0, 0);
	    y = strtol(params[3], 0, 0);
	    if (x < 0 || x >= MAX_GRID_X) {
		cerr << "Error: x parameter must be >=0 and < " << MAX_GRID_X << endl;
		break;
            }
	    if (y < 0 || y >= MAX_GRID_Y) {
		cerr << "Error: y parameter must be >=0 and < " << MAX_GRID_Y << endl;
		break;
            }
   	    err = move_item(name, x, y);
	    if (!err) {
		cout << "item " << name << " has been moved" << endl;
	    }
	    break;
	case locate_cmd:
	    name = params[1];
   	    err = location_of(name, x, y);
	    if (!err) {
		cout << "item " << name << " is located at: "
		     << x << "," << y << endl;
	    }
	    break;
	case spatial_cmd: {
	    // generate nbox from last 4 parameters
	    const coord_cnt = 4;
	    int coord[coord_cnt];
	    int i;
	    for (i = 0; i < coord_cnt; i++ ) {
		coord[i] = strtol(params[i+1], 0, 0);
	    }
	    {
		nbox_t box(2, coord);
		spatial_result_t result;
		err = spatial_query(box, result);
		if (!err) {
		    cout << "In box [" << coord[0] << "," << coord[1]
			 << " " << coord[2] << "," << coord[3] << "]"
			 << " there are " << result.found_cnt << " items."
			 << endl;
		    if (result.found_cnt > 0) {
			int print_cnt = MIN(MAX_SPATIAL_RESULT, result.found_cnt);
			cout << "The first " << print_cnt 
			     << " items found are:" << endl;
			item_t* it; // item iterator
			for (i = 0, it = result.items;
			     i < print_cnt; i++, it++) {
			    cout << it->name << " " << it->x << "," << it->y << endl;
			}
		    }
		}
	    }
	    }
	    break;
      	case quit_cmd:
	    quit = true;
	    break;
      	case help_cmd:
	    print_commands();
	    break;
	default:
	    cerr << "Internal Error at: " << __FILE__ << ":" << __LINE__ << endl;
	    exit(1);
	}
	if (err) {
	    //cerr << "Error: " << err << endl;
	    cerr << err << endl;
	    cerr << "Error: " << cmd->name << " command failed." << endl;
	}
    }
}
