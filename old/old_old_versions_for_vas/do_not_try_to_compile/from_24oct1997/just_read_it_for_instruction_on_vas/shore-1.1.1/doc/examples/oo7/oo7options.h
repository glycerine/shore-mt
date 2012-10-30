#include <w.h>
#include <option.h>

extern char 	*configfile;
extern option_t *configfile_opt;
extern option_t *debug_opt;
extern bool debugMode;

w_rc_t setup_options(option_group_t *options);

w_rc_t initialize( int &argc, char **argv, const char *usage);
