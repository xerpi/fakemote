/* mload.c (for PPC) (c) 2009, Hermes
 *
 * This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <elf.h>
#include "mload.h"

static s32 iosDestroyHeap(int heapid)
{
	return 0;
}

static const char mload_fs[] ATTRIBUTE_ALIGN(32) = "/dev/mload";

static s32 mload_fd = -1;
static s32 hid = -1;

/*--------------------------------------------------------------------------------------------------------------*/

// to init/test if the device is running

int mload_init()
{
	int n;

	if (hid < 0)
		hid = iosCreateHeap(0x800);

	if (hid < 0) {
		if (mload_fd >= 0)
			IOS_Close(mload_fd);

		mload_fd = -1;

		return hid;
	}

	if (mload_fd >= 0) {
		return 0;
	}

	for (n = 0; n < 20; n++) { // try 5 seconds
		mload_fd = IOS_Open(mload_fs, 0);

		if (mload_fd >= 0)
			break;

		usleep(250 * 1000);
	}

	if (mload_fd < 0) {
		if (hid >= 0) {
			iosDestroyHeap(hid);
			hid = -1;
		}
	}

	return mload_fd;
}

/*--------------------------------------------------------------------------------------------------------------*/

// to close the device (remember call it when rebooting the IOS!)

int mload_close()
{
	int ret;

	if (hid >= 0) {
		iosDestroyHeap(hid);
		hid = -1;
	}

	if (mload_fd < 0)
		return -1;

	ret = IOS_Close(mload_fd);

	mload_fd = -1;

	return ret;
}

/*--------------------------------------------------------------------------------------------------------------*/

// get the base and the size of the memory readable/writable to load modules

int mload_get_load_base(u32 *starlet_base, int *size)
{
	int ret;

	if (mload_init() < 0)
		return -1;

	ret = IOS_IoctlvFormat(hid, mload_fd, MLOAD_GET_LOAD_BASE, ":ii", starlet_base, size);

	return ret;
}

/*--------------------------------------------------------------------------------------------------------------*/

// load and run a module from starlet (it need to allocate MEM2 to send the elf file)
// the module must be a elf made with stripios

int mload_module(void *addr, int len)
{
	int ret;
	void *buf = NULL;


	if (mload_init() < 0)
		return -1;

	if (hid >= 0) {
		iosDestroyHeap(hid);
		hid = -1;
	}

	hid = iosCreateHeap(len + 0x800);

	if (hid < 0)
		return hid;

	buf = iosAlloc(hid, len);

	if (!buf) {
		ret = -1; goto out;
	}


	memcpy(buf, addr, len);

	ret = IOS_IoctlvFormat(hid, mload_fd, MLOAD_LOAD_ELF, ":d", buf, len);

	if (ret < 0)
		goto out;

	ret = IOS_IoctlvFormat(hid, mload_fd, MLOAD_RUN_ELF, ":");

	if (ret < 0) {
		ret = -666; goto out;
	}

out:
	if (hid >= 0) {
		iosDestroyHeap(hid);
		hid = -1;
	}

	return ret;
}

/*--------------------------------------------------------------------------------------------------------------*/

// load a module from the PPC
// the module must be a elf made with stripios

int mload_elf(const void *my_elf, data_elf *data_elf)
{
	int n, m;
	int p;
	u8 *adr;
	u32 elf = (u32)my_elf;
	const Elf32_Ehdr *head = my_elf;

	if (elf & 3)
		return -1; // aligned to 4 please!

	if ((head->e_ident[EI_MAG0] != ELFMAG0) ||
	    (head->e_ident[EI_MAG1] != ELFMAG1) ||
	    (head->e_ident[EI_MAG2] != ELFMAG2) ||
	    (head->e_ident[EI_MAG3] != ELFMAG3)) {
		return -1;
	}

	p = head->e_phoff;
	data_elf->start = (void *)head->e_entry;

	for (n = 0; n < head->e_phnum; n++) {
		Elf32_Phdr *phdr = (void *)(elf + p);
		p += sizeof(Elf32_Phdr);

		if (phdr->p_type == PT_NOTE) {
			adr = (void *)(elf + phdr->p_offset);

			if (getbe32(0) != 0)
				return -2; // bad info (sure)

			for (m = 4; m < phdr->p_memsz; m += 8) {
				switch (getbe32(m)) {
				case 0x9:
					data_elf->start = (void *)getbe32(m + 4);
					break;
				case 0x7D:
					data_elf->prio = getbe32(m + 4);
					break;
				case 0x7E:
					data_elf->size_stack = getbe32(m + 4);
					break;
				case 0x7F:
					data_elf->stack = (void *)(getbe32(m + 4));
					break;
				}
			}
		} else if (phdr->p_type == PT_LOAD && phdr->p_memsz != 0 && phdr->p_vaddr != 0) {
			// printf("Segment: p_memsz: 0x%04x, p_filesz: 0x%04x\n", phdr->p_memsz, phdr->p_filesz);
			if (mload_memset((void *)phdr->p_vaddr, 0, phdr->p_memsz) < 0)
				return -1;
			if (mload_seek(phdr->p_vaddr, SEEK_SET) < 0)
				return -1;
			if (mload_write((void *)(elf + phdr->p_offset), phdr->p_filesz) < 0)
				return -1;
		}
	}

	return 0;
}

/*--------------------------------------------------------------------------------------------------------------*/

// run one thread (you can use to load modules or binary files)

int mload_run_thread(void *starlet_addr, void *starlet_top_stack, int stack_size, int priority)
{
	int ret;


	if (mload_init() < 0)
		return -1;

	ret = IOS_IoctlvFormat(hid, mload_fd, MLOAD_RUN_THREAD, "iiii:", starlet_addr, starlet_top_stack, stack_size, priority);


	return ret;
}

/*--------------------------------------------------------------------------------------------------------------*/

// stops one starlet thread

int mload_stop_thread(int id)
{
	int ret;

	if (mload_init() < 0)
		return -1;

	ret = IOS_IoctlvFormat(hid, mload_fd, MLOAD_STOP_THREAD, "i:", id);

	return ret;
}

/*--------------------------------------------------------------------------------------------------------------*/

// continue one stopped starlet thread

int mload_continue_thread(int id)
{
	int ret;

	if (mload_init() < 0)
		return -1;

	ret = IOS_IoctlvFormat(hid, mload_fd, MLOAD_CONTINUE_THREAD, "i:", id);

	return ret;
}
/*--------------------------------------------------------------------------------------------------------------*/

// fix starlet address to read/write (uses SEEK_SET, etc as mode)

int mload_seek(int offset, int mode)
{
	if (mload_init() < 0)
		return -1;

	return IOS_Seek(mload_fd, offset, mode);
}

/*--------------------------------------------------------------------------------------------------------------*/

// read bytes from starlet (it update the offset)

int mload_read(void *buf, u32 size)
{
	if (mload_init() < 0)
		return -1;

	return IOS_Read(mload_fd, buf, size);
}

/*--------------------------------------------------------------------------------------------------------------*/

// write bytes from starlet (it update the offset)

int mload_write(const void *buf, u32 size)
{
	if (mload_init() < 0)
		return -1;

	return IOS_Write(mload_fd, buf, size);
}

/*--------------------------------------------------------------------------------------------------------------*/

// fill a block (similar to memset)

int mload_memset(void *starlet_addr, int set, int len)
{
	int ret;

	if (mload_init() < 0)
		return -1;

	ret = IOS_IoctlvFormat(hid, mload_fd, MLOAD_MEMSET, "iii:", starlet_addr, set, len);


	return ret;
}

/*--------------------------------------------------------------------------------------------------------------*/

// to get IOS base for dev/es  to create the cIOS

int mload_get_IOS_base()
{
	int ret;

	if (mload_init() < 0)
		return -1;

	ret = IOS_IoctlvFormat(hid, mload_fd, MLOAD_GET_IOS_INFO, ":");

	return ret;
}

int mload_set_log_mode(u32 mode)
{
	int ret;

	if (mload_init() < 0)
		return -1;

	ret = IOS_IoctlvFormat(hid, mload_fd, MLOAD_SET_LOG_MODE, "i:", mode);

	return ret;
}

int mload_get_log_buffer(void *addr, u32 max_size)
{
	int ret;

	if (mload_init() < 0)
		return -1;

	ret = IOS_IoctlvFormat(hid, mload_fd, MLOAD_GET_LOG_BUFFER, ":d", addr, max_size);

	return ret;
}

int mload_get_log_buffer_and_empty(void *addr, u32 max_size)
{
	int ret;

	if (mload_init() < 0)
		return -1;

	ret = IOS_IoctlvFormat(hid, mload_fd, MLOAD_GET_LOG_BUFFER_AND_EMPTY, ":d", addr, max_size);

	return ret;
}

int mload_set_log_ringbuf(u32 head_paddr, u32 tail_paddr, u32 data_paddr, u32 size)
{
	int ret;

	if (mload_init() < 0)
		return -1;

	ret = IOS_IoctlvFormat(hid, mload_fd, MLOAD_SET_LOG_RINGBUFFER, "iiii:", head_paddr, tail_paddr, data_paddr, size);

	return ret;
}
