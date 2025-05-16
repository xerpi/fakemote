#ifndef EGC_OGC_ARM_H
#define EGC_OGC_ARM_H

#ifdef __arm__

#include "egc_types.h"
#define PSR_I (1 << 7)

__attribute__((target("arm"))) u32 get_cpsr(void);
__attribute__((target("arm"))) void set_cpsr_c(u32 cpsr);

#endif /* __arm__ */

#endif
