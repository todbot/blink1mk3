#ifndef BLINK1_DEBUG
#define BLINK1_DEBUG

#define UNUSED(x) (void)(x)

#if DEBUG > 0

// used when sprintf()-ing to leuart
char dbgstr[50];

// for tiny printf
int myputc ( void* p, char c) {
  (void)p;
  return write_char(c);
}

// a debug printf out the leuart
#define dbg_setup() setupLeuart()
#define dbg_printf(...) sprintf(dbgstr,__VA_ARGS__); write_str(dbgstr);
#define dbg_str(s) write_str(s)
#define dbg_ch(c) write_char(c)

#else

#define dbg_setup()
#define dbg_printf(...)
#define dbg_str(s)
#define dbg_ch(c) 

#endif


#endif
