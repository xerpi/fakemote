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

static s32 arm_decode_bl_offset_thumb2(u32 insn)
{
	u16 first = (insn >> 16) & 0xFFFF;
	u16 second = insn & 0xFFFF;

	u32 imm10 = first & 0x3ff;
	u32 imm11 = second & 0x7ff;
	u32 s = (first >> 10) & 1;
	u32 j1 = (second >> 13) & 1;
	u32 j2 = (second >> 11) & 1;
	u32 i1 = !(j1 ^ s);
	u32 i2 = !(j2 ^ s);

	return (((s32)(((s << 24) | (i1 << 23) |
	        (i2 << 22) | (imm10 << 12) |
	        (imm11 << 1)) << 8)) >> 8) + 4;
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
	//printf("mload_set_log_ringbuf(): %d\n", ret);

	ret = mload_set_log_mode(DEBUG_BUFFER);
	//printf("mload_set_log_mode(): %d\n", ret);

	data_elf info;
	ret = mload_elf(test_module_elf_start, &info);
	printf("mload_elf(): %d\n", ret);
	/*printf("  start: %p\n", info.start);
	printf("  prio: %d\n", info.prio);
	printf("  stack: %p\n", info.stack);
	printf("  size_stack: 0x%x\n", info.size_stack);*/
	int thid = mload_run_thread(info.start, info.stack, info.size_stack, info.prio);
	printf("mload_run_thread(): %d\n", thid);

	printf("Entering main loop\n");
	//for (int i = 0; i < 30; i++) puts("");

	#define LOG_SIZE (4096)
	char *log = memalign(32, LOG_SIZE);
	if (!log) run = 0;
	char prev_log[LOG_SIZE];
	memset(log, 0, LOG_SIZE);
	memset(prev_log, 0, sizeof(prev_log));
	u32 last_len = 0;

	while (run) {
		WPAD_ScanPads();
		u32 pressed = WPAD_ButtonsDown(0);
		if (pressed & WPAD_BUTTON_HOME)
			run = 0;

		if (0) {
			/* Log ringbuffer consumer */
			u32 cnt_to_end;
			do {
				u32 head = read32((uintptr_t)rb_head_uc);
				u32 tail = read32((uintptr_t)rb_tail_uc);

				cnt_to_end = CIRC_CNT_TO_END(head, tail, rb_size);

				if (cnt_to_end != 0) {
					static char buf[4096+1];

					memset(buf, 0, sizeof(buf));
					//DCInvalidateRange(rb_data + tail, cnt_to_end);
					DCInvalidateRange(rb_data, rb_size);
					memcpy(buf, rb_data + tail, cnt_to_end);
					//buf[cnt_to_end + 1] = '\0';

					//printf("head: 0x%x, tail: 0x%x  ->  cnt_to_end: 0x%x\n", head, tail, cnt_to_end);
					printf("%s\n", buf);

					tail = (tail + cnt_to_end) & (rb_size - 1);
					write32((uintptr_t)rb_tail_uc, tail);
				}
			} while (cnt_to_end > 0);
		} else {
			memset(log, 0, LOG_SIZE);
			DCFlushRange(log, LOG_SIZE);
			ret = mload_get_log_buffer_and_empty(log, LOG_SIZE);
			if (ret > 0) {
				DCInvalidateRange(log, LOG_SIZE);
				printf("%s", log);
			}
			usleep(1000);
		}

		VIDEO_WaitVSync();
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
