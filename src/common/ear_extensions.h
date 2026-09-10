#ifndef EAR_EXTENSIONS_H
#define EAR_EXTENSIONS_H

#include <common/system/time.h>

// This class contains a code that has to be moved to EAR
#define timestamp_is_zero(ts) (ts.tv_sec == 0 && ts.tv_nsec == 0)

#endif //EAR_EXTENSIONS_H
