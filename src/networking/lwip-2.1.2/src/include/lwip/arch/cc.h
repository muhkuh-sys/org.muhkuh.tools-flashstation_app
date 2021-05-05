#if CFG_LWIP_VERBOSE!=0
#       include "debug_print.h"

#       define LWIP_PLATFORM_DIAG(x) {debug_uprintf x;}
#       define LWIP_PLATFORM_ASSERT(x) {debug_uprintf("Assertion \"%s\" failed at line %d in %s\n", x, __LINE__, __FILE__); while(1){};}
#else
#       define LWIP_PLATFORM_DIAG(x)
#       define LWIP_PLATFORM_ASSERT(x) {while(1){};}
#endif


#define LWIP_RAND() (rng_get_value())
#define sys_now systime_get_ms


#define LWIP_NO_INTTYPES_H 1
#define X8_F  "02x"
#define U16_F "d"
#define S16_F "d"
#define X16_F "04d"
#define U32_F "d"
#define S32_F "d"
#define X32_F "08x"
#define SZT_F "08x"
