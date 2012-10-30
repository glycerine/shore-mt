/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: option.h,v 1.17 1997/05/19 19:41:06 nhall Exp $
 */
#ifndef OPTION_H
#define OPTION_H

#include <fstream.h>
#include <strstream.h>

#ifdef __GNUG__
#pragma interface
#endif

/*
    Option Configuration Facility

The configuration option facility consists of 3 classes: option_t,
option_group_t, and option_file_scan_t.  Objects of type option_t
contain information about options.  An option has a string
name and value.  An option_group_t manages a related group
of options.  An option_file_scan_t is used to parse a file
containing option configuration information.
*/

#ifndef __opt_error_def_h__
#include "opt_error_def.h"
#endif

class option_group_t;

//
// option_t Class:
//
// Description:
//   Information about an option is stored in a option_t object.
//   All options have a name stored in the _name field.
//   The _possibleValue field is used to print example possible
//   values for usage.  The _default_value field hold a default value
//   if _required is false.  It is also used to initialize _value.
//   Member _description is printed for long usage information.
//   Member _required indicates that the option must be set.
//   Member _set is set to true by set_value().
//   Member _value holds the latest value from set_value().
//   If the option is not required and _default_value is not NULL then
//   valueM will be set to _default_value if set_value() is not called.
//
class option_t : public w_base_t {
friend option_group_t;
public:

    // returns true if the option name matches matchName
    bool	match(const char* matchName, bool exact=false);

    // set the value of an option if it is not already set
    // or if overRide is true.
    // error messages will be "printed" to errstream if
    // non-null;
    // a value of 0 indicates un-set
    w_rc_t	set_value(const char* value, bool overRide, ostream* err_stream);

    // get information about an option
    const char*	value() 	{ return _value;}
    bool	is_set()	{ return _set; }
    bool	is_required()	{ return _required; }
    const char*	name()		{ return _name; }
    const char*	possible_values(){ return _possible_values; }	
    const char*	default_value()	{ return _default_value; }	
    const char*	description()	{ return _description; }	

    // These functions are used to write "call-back" functions
    // that are called when a value is set.  See code
    // for set_value_bool() for examples of how to use them.
    void	markSet(bool s)	{ _set = s; }
    w_rc_t	copyValue(const char* value);
    w_rc_t	concatValue(const char* value);

    // Type for "call back" functions called when a value is set
    typedef w_rc_t (*OptionSetFunc)(option_t*, const char * value,
    		    ostream* err_stream);

    // Standard call back functions for basic types
    static w_rc_t set_value_bool(option_t* opt, const char* value, ostream* err_stream);
    static w_rc_t set_value_long(option_t* opt, const char* value, ostream* err_stream);
    static w_rc_t set_value_charstr(option_t* opt, const char* value, ostream* err_stream);

    // function to convert a string to a bool (similar to strtol()).
    // first character is checked for t,T,y,Y for true
    // and f,F,n,N for false.
    // bad_str is set to true if none of these match (and 
    // false will be returned)
    static bool	str_to_bool(const char* str, bool& bad_str);

private:

    // These functions are called by option_group_t::add_option().
    NORET	option_t();
    NORET	~option_t();
    		// initialize an option_t object
    w_rc_t	init(const char* name, const char* newPoss,
		     const char* default_value, const char* description,
		     bool required, OptionSetFunc callBack);

    const char*	_name;			// name of the option
    const char*	_possible_values;	// example possible values
    const char*	_default_value;		// default value
    const char*	_description;		// description string
    bool	_required;		// must option be set
    bool	_set;			// option has been set
    char*	_value;			// value for the option
    w_link_t	_link;			// link list of options 

    /*
     *      call-back function to call when option value is set
     *
     *      Call back functions should return 0 on success or a on-zero
     *      error code on failure.  This error code should not confict
     *      with the option error codes above.
     */
    OptionSetFunc _setFunc;

};

//
// option_group_t Class
//
// Description:
//   An option_group_t manages a set of options.  An option group has
//   a classification hierarchy associated with it.  Each level
//   of the hierarchy is given a string name.  Levels are added
//   with add_class_level().  The levels are used when looking up
//   an option with lookup_by_class().  The level hierarchy is printed
//   in the form: "level1.level2.level3."  A complete option name is
//   specified by "level1.level2.level3.optionName:".  A common
//   convention for level names is:
//	programtype.programname
//   where programtype is indicates the general type of the program
//   and programname is the file name of the program.
//
class option_group_t : public w_base_t {
public:
    NORET	option_group_t(int max_class_levels);
    NORET 	~option_group_t();

    // add_class_level is used to add a level name
    w_rc_t	add_class_level(const char* name);

    // Use add_option to add an option to the group.
    // The set_func parameter indicates what function to call
    // when the option is set.  Use one of the functions
    // from option_t or write your own.
    w_rc_t	add_option(const char* name, const char* possible_values,
		       const char* default_value,
		       const char* description, bool required,
		       option_t::OptionSetFunc set_func,
		       option_t*& new_opt);

    // lookup and option by name.  abbreviations are allowed if
    // they are unique and if exact is false
    w_rc_t	lookup(const char* name, bool exact, option_t*&);


    // lookup option by class name and option name.
    // opt_class_name is assumed to be a string of the form
    //    level1.level2.optionname.
    // A "?" can be used as a wild card for any single level name.
    // A "*" can be used as a wild card for any number of level name.
    w_rc_t	lookup_by_class(const char* opt_class_name, option_t*&,
		bool exact=false);

    // set the value of an option if it is not already set
    // or if overRide is true.
    // error messages will be "printed" to errstream if
    // non-null;
    // a value of 0 indicates un-set
    w_rc_t	set_value(const char* name, bool exact,
			  const char* value, bool overRide,
			  ostream* err_stream);

    void	print_usage(bool longForm, ostream& err_stream);
    void	print_values(bool longForm, ostream& err_stream);

    // check that all required options are set.
    // return OPTERR_NotSet if any are not
    // print information about each unset option to err_stream
    w_rc_t 	check_required(ostream* err_stream);

    // parse_command_line searches the command line for any
    // options in the group (options are assumed to be preceded by "-"
    // and are only recognized if they are longer than min_len since
    // options may be abbreviated).  Any options found are
    // removed from argv and argc is adjusted accordingly.
    w_rc_t 	parse_command_line(char** argv, int& argc, size_t min_len, ostream* error_stream);

    // use this for scanning the list of options
    w_list_t<option_t>& option_list() {return _options;}

    int		num_class_levels(){ return _numLevels; }	
    const char*	class_name()	{ return _class_name; }	

private:
    w_list_t<option_t>  _options;
    char*		_class_name;
    // array of offsets into _class_name
    char** 		_levelLocation;
    int			_maxLevels;
    int			_numLevels;

    static bool	_error_codes_added;

	// disable copy operator
	NORET option_group_t(option_group_t const &); 
};

/*
 * Class option_file_scan_t supports scaning a text file containing
 * values for options.
 * Each line of the file is either a comment or an
 * option value setting.
 * 
 * A comment line begins with "!" or "#".
 *
 * An option value setting line has the form:
 *     level1.level2.optionname: value of option
 *
 * level1.level2.optionname is anything acceptable to
 * option_group_t::lookup_by_class().  The value of the option
 * is the string beginning with the first non-white space character
 * after the ":" and ending with last non-white space character in
 * the line.
 */ 
class option_file_scan_t : public w_base_t {
public:
    NORET	option_file_scan_t(const char* opt_file_path, option_group_t* opt_group);
    NORET	~option_file_scan_t();

    /*
     * Scan the options file reporting errors to err_stream.
     * If over_ride is false, new values of options will be ignored
     * if the option value has already been set.
     * The exact parameter means that mispellings or abbreviations
     * of option names will result in an error.
     */
    w_rc_t 	scan(bool over_ride, ostream& err_stream, 
	bool exact=false, bool mismatch_ok=false);

protected:
    option_group_t*	_optList;
    const char*		_fileName;
    istream*		_input;
    int			_lineNum;
    char*		_line;
    size_t		_maxLineLen;
};

#endif /* OPTION_H */
