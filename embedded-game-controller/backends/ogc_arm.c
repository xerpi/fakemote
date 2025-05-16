#ifdef __arm__

#include "ogc_arm.h"

__attribute__((target("arm")))
u32 get_cpsr(void)
{
	u32 cpsr;
	asm volatile("mrs %0, cpsr" : "=r" (cpsr) : );
	return cpsr;
}

__attribute__((target("arm")))
void set_cpsr_c(u32 cpsr)
{
	asm volatile("msr cpsr_c, %0" : : "r" (cpsr) : "memory");
}
#endif

