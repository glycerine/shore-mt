#include <stdio.h>
#include "sdl_fct.h"
extern void fprintf(...);
int
main(int ac, char* av[])
{
	sdl_main_fctpt sdl_app= 0;
	if (ac >=2  )
	{
		sdl_app = sdl_fct::lookup(av[1]);
		if (sdl_app==0)
		{
			fprintf(stderr," no sdl function %s found\n",av[1]);
			return(-1);
		}
		else
		{
			int sdl_rc;
			sdl_rc = (sdl_app)(ac-1,av +1);
			fprintf(stderr, "Done running %s\n", av[1]);
			return sdl_rc;
		}
	}
	fprintf(stderr,"usage: <sdl main function name> <args>\n");
	exit(-1);
}
