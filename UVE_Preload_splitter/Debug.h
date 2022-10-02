#ifndef Debug_h_
#define Debug_h_

//#define DEBUG_ENABLE 1
//#define DEBUG_PRINT_ATLAS_ENTRY 1

#ifdef DEBUG_ENABLE
#define DEBUG_PAUSE { printf_s("DEBUG PAUSE, PRESS ENTER\n"); char dbp = 0; dbp = getchar(); }
#define DEBUG_PAUSE_EXIT { printf_s("DEBUG PAUSE, PRESS ENTER\n"); char dbp = 0; dbp = getchar(); exit(0); }
#define DEBUG_PRINTVAL(value, format_type) { printf_s("%s="##format_type"\n", #value, value); }
#else
#define DEBUG_PAUSE ;
#define DEBUG_PAUSE_EXIT ;
#define DEBUG_PRINTVAL(value, format_type) ;
#endif

#endif // !Debug_h_
