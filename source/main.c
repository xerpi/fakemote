#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <ogc/machine/processor.h>
#include "mload.h"

/* From linux/circ_buf.h */
/* Return count in buffer.  */
#define CIRC_CNT(head,tail,size) (((head) - (tail)) & ((size)-1))

/* Return space available, 0..size-1.  We always leave one free char
   as a completely full buffer has head == tail, which is the same as
   empty.  */
#define CIRC_SPACE(head,tail,size) CIRC_CNT((tail),((head)+1),(size))

/* Return count up to the end of the buffer.  Carefully avoid
   accessing head and tail more than once, so they can change
   underneath us without returning inconsistent results.  */
#define CIRC_CNT_TO_END(head,tail,size) \
	({int end = (size) - (tail); \
	  int n = ((head) + end) & ((size)-1); \
	  n < end ? n : end;})

/* Return space available up to the end of the buffer.  */
#define CIRC_SPACE_TO_END(head,tail,size) \
	({int end = (size) - 1 - (head); \
	  int n = (end + (tail)) & ((size)-1); \
	  n <= end ? n : end+1;})

extern void *_binary____arm_test_module_elf_start;
extern void *_binary____arm_test_module_elf_size;

static const void *test_module_elf_start = &_binary____arm_test_module_elf_start;
static const u32 test_module_elf_size = (u32)&_binary____arm_test_module_elf_size;

static int run = 1;

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

	int ret = mload_init();
	printf("mload_init(): %d\n", ret);
	
	u32 *rb_head = memalign(32, sizeof(u32));
	u32 *rb_tail = memalign(32, sizeof(u32));
	u32 *rb_head_uc = (void *)((uintptr_t)rb_head | SYS_BASE_UNCACHED);
	u32 *rb_tail_uc = (void *)((uintptr_t)rb_tail | SYS_BASE_UNCACHED);
	u32 rb_size = 4096;
	u8 *rb_data = memalign(32, rb_size);
	
	*rb_head_uc = 0;
	*rb_tail_uc = 0;
	memset(rb_data, 0, rb_size);
	DCFlushRange(rb_data, rb_size);

	ret = mload_set_log_ringbuf(MEM_VIRTUAL_TO_PHYSICAL(rb_head),
	                            MEM_VIRTUAL_TO_PHYSICAL(rb_tail),
	                            MEM_VIRTUAL_TO_PHYSICAL(rb_data),
	                            rb_size);
	printf("mload_set_log_ringbuf(): %d\n", ret);

	ret = mload_set_log_mode(DEBUG_RINGBUF);
	printf("mload_set_log_mode(): %d\n", ret);

	//u32 starlet_base;
	//int size;
	//ret = mload_get_load_base(&starlet_base, &size);
	//printf("base: 0x%08X, size: 0x%08X\n", starlet_base, size);

	data_elf info;
	ret = mload_elf(test_module_elf_start, &info);
	printf("mload_elf(): %d\n", ret);
	printf("  start: %p\n", info.start);
	printf("  prio: %d\n", info.prio);
	printf("  stack: %p\n", info.stack);
	printf("  size_stack: 0x%x\n", info.size_stack);
	int thid = mload_run_thread(info.start, info.stack, info.size_stack, info.prio);
	printf("mload_run_thread(): %d\n", thid);

#if 0
	#define BL_ADDR	0x138b23c6

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

	printf("\nEntering main loop\n");

	//#define LOG_SIZE 4096
	//char *log = memalign(32, LOG_SIZE);
	//char last[LOG_SIZE];
	//u32 i = 0;

	while (run) {
		WPAD_ScanPads();
		u32 pressed = WPAD_ButtonsDown(0);
		if (pressed & WPAD_BUTTON_HOME)
			run = 0;

		/* Log ringbuffer consumer */
		u32 head = read32((uintptr_t)rb_head_uc);
		u32 tail = read32((uintptr_t)rb_tail_uc);
		u32 cnt_to_end = CIRC_CNT_TO_END(head, tail, rb_size);
		
		if (cnt_to_end != 0) {
			static char buf[4096+1];
			
			DCInvalidateRange(rb_data + tail, cnt_to_end);
			memcpy(buf, rb_data + tail, cnt_to_end);
			buf[cnt_to_end + 1] = '\0';
			
			//printf("head: 0x%x, tail: 0x%x  ->  cnt_to_end: 0x%x\n", head, tail, cnt_to_end);
			puts(buf);
			
			tail = (tail + cnt_to_end) & (rb_size - 1);
			write32((uintptr_t)rb_tail_uc, tail);
		}

		/*ret = mload_get_log_buffer(log, LOG_SIZE);
		//printf("mload_get_log_buffer(): %d\n", ret);
		if (ret > 0) {
			if (ret != i) {
				DCInvalidateRange(log, LOG_SIZE);
				printf("  > %s", log);
				//i = (i + ret) % LOG_SIZE;
				i = ret;
			}
		}*/

		//VIDEO_WaitVSync();
	}

	printf("\n\nExiting...\n");

	//free(log);

	ret = mload_stop_thread(thid);
	printf("mload_stop_thread(): %d\n", thid);

	ret = mload_close();
	printf("mload_close(): %d\n", ret);
	
	free(rb_head);
	free(rb_tail);
	free(rb_data);

	return 0;
}
