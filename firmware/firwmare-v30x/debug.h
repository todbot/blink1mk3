#ifndef BLINK1_DEBUG
#define BLINK1_DEBUG

#define UNUSED(x) (void)(x)

#if DEBUG > 0

// used when sprintf()-ing to leuart
char dbgstr[50];

// for tiny printf
void myputc ( void* p, char c) {
  (void)p;  write_char(c);
}

// a debug printf out the leuart
#define dbg_printf(...) sprintf(dbgstr,__VA_ARGS__); write_str(dbgstr);
#define dbg_str(s) write_str(s)

#else

#define dbg_printf(...)
#define dbg_str(s)

#endif


#endif
