/^typedef char \*Path/s=char=const char=
/^#include "\/tmp\/msg.h"/s="\/tmp/\msg.h"="msg.h"=
1,$s=^\(bool_t xdr_\)\([^\(]*\).*$=EXTERNC \1\2 (XDR *,\2 *);=
1,$s=^extern \(.[a-zA-Z_0-9][a-zA-Z_0-9]*_reply \*\)\([a-zA-Z_0-9][a-zA-Z_0-9]*\)_1();=EXTERNC \1\2_1(\2_arg *, void *);=

