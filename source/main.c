#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <gccore.h>
#include <wiiuse/wpad.h>
#include "mload.h"

extern void *_binary____arm_test_module_elf_start;
extern void *_binary____arm_test_module_elf_size;

static const void *test_module_elf_start = &_binary____arm_test_module_elf_start;
static const u32 test_module_elf_size = (u32)&_binary____arm_test_module_elf_size;

static int run = 1;

static unsigned long arm_gen_branch_thumb2(unsigned long pc,
					   unsigned long addr, bool link)
{
	unsigned long s, j1, j2, i1, i2, imm10, imm11;
	unsigned long first, second;
	long offset;

	offset = (long)addr - (long)(pc + 4);
	if (offset < -16777216 || offset > 16777214)
		return 0;

	s	= (offset >> 24) & 0x1;
	i1	= (offset >> 23) & 0x1;
	i2	= (offset >> 22) & 0x1;
	imm10	= (offset >> 12) & 0x3ff;
	imm11	= (offset >>  1) & 0x7ff;

	j1 = (!i1) ^ s;
	j2 = (!i2) ^ s;

	first = 0xf000 | (s << 10) | imm10;
	second = 0x9000 | (j1 << 13) | (j2 << 11) | imm11;
	if (link)
		second |= 1 << 14;
		
	first  = __builtin_bswap16(first);
	second = __builtin_bswap16(second);

	return __builtin_bswap32(first | (second << 16));
}

static void button_pressed()
{
	run = 0;
}

static int starlet_read32(u32 addr, u32 *data)
{
	int ret;
	ret = mload_seek(addr, SEEK_SET);
	if (ret < 0)
		return ret;
	return mload_read(data, sizeof(u32));
}

static int starlet_write32(u32 addr, const u32 *data)
{
	int ret;
	ret = mload_seek(addr, SEEK_SET);
	if (ret < 0)
		return ret;
	return mload_write(data, sizeof(u32));
}

int main(int argc, char **argv)
{
	void *xfb;
	GXRModeObj *rmode;

	IOS_ReloadIOS(249);

	SYS_SetResetCallback(button_pressed);
	SYS_SetPowerCallback(button_pressed);

	VIDEO_Init();
	WPAD_Init();

	rmode = VIDEO_GetPreferredMode(NULL);
	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	console_init(xfb, 20, 20, rmode->fbWidth, rmode->xfbHeight, rmode->fbWidth * VI_DISPLAY_PIX_SZ);
	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(xfb);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if (rmode->viTVMode&VI_NON_INTERLACE)
		VIDEO_WaitVSync();

	printf("\x1b[2;0H");
	printf("Hello World!\n");
	
	printf("test_module_elf_start: %p\n", test_module_elf_start);
	printf("test_module_elf_size: 0x%x\n", test_module_elf_size);
	
	int ret = mload_init();
	printf("mload_init(): %d\n", ret);
	
	//u32 starlet_base;
	//int size;
	//ret = mload_get_load_base(&starlet_base, &size);
	//printf("base: 0x%08X, size: 0x%08X\n", starlet_base, size);
	
	// Copy module to heap
	void *mod_heap = memalign(32, test_module_elf_size);
	memcpy(mod_heap, test_module_elf_start, test_module_elf_size);
	
	data_elf info;
	ret = mload_elf((void *)mod_heap, &info);
	printf("mload_elf(): %d\n", ret);
	printf("  start: %p\n", info.start);
	printf("  prio: %d\n", info.prio);
	printf("  stack: %p\n", info.stack);
	printf("  size_stack: 0x%x\n", info.size_stack);
	int thid = mload_run_thread(info.start, info.stack, info.size_stack, info.prio);
	printf("mload_run_thread(): %d\n", thid);
	
	//usleep(350 * 1000);
	
	#define BL_ADDR	0x138b23c6

#if 0
	u32 orig_bl_insn;
	u32 new_bl_insn = arm_gen_branch_thumb2(BL_ADDR, 0x138b365c, true);
	printf("new_bl_insn: 0x%08X\n", new_bl_insn);

	//u32 *data = (u32 *)memalign(32, 128);
	int hid = iosCreateHeap(0x800);
	u32 *data = iosAlloc(hid, 128);
	
	ret = starlet_read32(BL_ADDR, data);
	printf("starlet_read32(): %d, data: 0x%08X\n", ret, *data);
	orig_bl_insn = *data;
	
	*data = new_bl_insn;
	//ret = starlet_write32(BL_ADDR, data);
	printf("starlet_write32(): %d, data: 0x%08X\n", ret, *data);
	
	//free(data);
#endif
	int k = 0;
	while (run) {
		printf("%d\n", k++);
		WPAD_ScanPads();
		if (k == 10)
			mload_stop_thread(thid);

		u32 pressed = WPAD_ButtonsDown(0);

		if (pressed & WPAD_BUTTON_HOME)
			run = 0;

		VIDEO_WaitVSync();
	}

	printf("\n\nExiting...\n");
	
	ret = mload_stop_thread(thid);
	printf("mload_stop_thread(): %d\n", thid);
	
	ret = mload_close();
	printf("mload_close(): %d\n", ret);

	return 0;
}
