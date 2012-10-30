#ifdef I860

char *io_fs_root(char *, char*);
int physicalNode(void);

#define MAXPHYS 142
short phys_to_io[MAXPHYS] = {
  -1,  -1,   1,   2,   3,   4,   5,   6,   7,   8,  -1,   0,
  -1,  -1,   9,  10,  11,  12,  13,  14,  15,  16,  -1,  -1,
  -1,  -1,  17,  18,  19,  20,  21,  22,  23,  24,  -1,  -1,
  -1,  -1,  25,  26,  27,  28,  29,  30,  31,  32,  -1,  -1,
  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  33,
  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
  -1,  -1,  34,  35,  36,  37,  38,  39,  40,  41,  -1,  -1,
  -1,  -1,  42,  43,  44,  45,  46,  47,  48,  49,  -1,  -1,
  -1,  -1,  50,  51,  52,  53,  54,  55,  56,  57,  -1,  -1,
  -1,  -1,  58,  59,  60,  61,  62,  63,  64,  65,};
                        

int physicalNode(){
        int physnode=_myphysnode();

        if (physnode < 0 || physnode >= MAXPHYS) {
                printf("XXX ack _ionode(%d) unknown node!\n", physnode);
                physnode = 0;
        }

        return(phys_to_io[physnode]);
}
#endif
