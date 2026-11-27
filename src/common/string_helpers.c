/*
 * SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "metrics_types.h"
#include <common/string_helpers.h>

#if 0
static int isnum(const char *string)
{
    if (!string) {
        return 0;
    }
    while (isspace((unsigned char) *string)) {
        string++;
    }
    if (*string == '\0') {
        return 0;
    }
    while (*string && !isspace((unsigned char) *string)) {
        if (!isdigit((unsigned char) *string)) {
            return 0;
        }
        string++;
    }
    return 1;
}
#endif

int label_to_id(const char *label)
{
    if (is(label, MET_NAME(All)     )) { return  All;        }
    if (is(label, MET_NAME(Metric)  )) { return  Metric;     }
    if (is(label, MET_NAME(Met)     )) { return  Met;        }
    if (is(label, MET_NAME(Actuator))) { return  Actuator;   }
    if (is(label, MET_NAME(Act)     )) { return  Act;        }
    if (is(label, MET_NAME(Cpu)     )) { return  Cpu;        }
    if (is(label, MET_NAME(Thread)  )) { return  Thread;     }
    if (is(label, MET_NAME(Core)    )) { return  Core;       }
    if (is(label, MET_NAME(Pp0)     )) { return  Pp0;        }
    if (is(label, MET_NAME(Socket)  )) { return  Socket;     }
    if (is(label, MET_NAME(Pkg)     )) { return  Pkg;        }
    if (is(label, MET_NAME(Package) )) { return  Package;    }
    if (is(label, MET_NAME(Uncore)  )) { return  Uncore;     }
    if (is(label, MET_NAME(Pp1)     )) { return  Pp1;        }
    if (is(label, MET_NAME(Mem)     )) { return  Mem;        }
    if (is(label, MET_NAME(Dram)    )) { return  Dram;       }
    if (is(label, MET_NAME(Gpu)     )) { return  Gpu;        }
    if (is(label, MET_NAME(Pow)     )) { return  Pow;        }
    if (is(label, MET_NAME(Pwr)     )) { return  Pwr;        }
    if (is(label, MET_NAME(Power)   )) { return  Power;      }
    if (is(label, MET_NAME(Energy)  )) { return  Energy;     }
    if (is(label, MET_NAME(Freq)    )) { return  Freq;       }
    if (is(label, MET_NAME(Frequency))) { return Frequency;  }
    if (is(label, MET_NAME(AccFreq) )) { return  AccFreq;    }
    if (is(label, MET_NAME(Bwidth)  )) { return  Bwidth;     }
    if (is(label, MET_NAME(Bandwidth))) { return Bandwidth;  }
    if (is(label, MET_NAME(Get)     )) { return Get;         }
    if (is(label, MET_NAME(Set)     )) { return Set;         }
    if (is(label, MET_NAME(Info)    )) { return Info;        }
    return 0;
}