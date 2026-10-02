#ifndef EAR_EXTENSIONS_H
#define EAR_EXTENSIONS_H

#include <string.h>

#define is(s1, s2) (strcasecmp(s1, s2) == 0)

int label_to_id(const char *label);

#endif //EAR_EXTENSIONS_H
