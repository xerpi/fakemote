/*   
	Custom IOS Library

	Copyright (C) 2008 neimod.
	Copyright (C) 2009 WiiGator.
	Copyright (C) 2009 Waninkoko.
	Copyright (C) 2010 Hermes.
	Copyright (C) 2011 davebaol.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/


/*
 * Macro
 */
.macro syscall num name
	.code 32
	.global \name
\name:
	.long 0xE6000010 + (\num << 5)
	bx lr
.endm


/*
 * IOS syscalls
 */
	syscall 0x00, os_thread_create
	syscall 0x01, os_thread_joint
	syscall 0x02, os_thread_cancel
	syscall 0x03, os_get_thread_id
	syscall 0x04, os_get_parent_thread_id
	syscall 0x05, os_thread_continue
	syscall 0x06, os_thread_stop
	syscall 0x07, os_thread_yield
	syscall 0x08, os_thread_get_priority
	syscall 0x09, os_thread_set_priority
	syscall 0x0a, os_message_queue_create
	syscall 0x0b, os_message_queue_destroy
	syscall 0x0c, os_message_queue_send 
	syscall 0x0d, os_message_queue_send_now
	syscall 0x0e, os_message_queue_receive
	syscall 0x0f, os_register_event_handler
	syscall 0x10, os_unregister_event_handler
	syscall 0x11, os_create_timer
	syscall 0x12, os_restart_timer
	syscall 0x13, os_stop_timer
	syscall 0x14, os_destroy_timer
	syscall 0x15, os_timer_now
	syscall 0x16, os_heap_create
	syscall 0x17, os_heap_destroy
	syscall 0x18, os_heap_alloc
	syscall 0x19, os_heap_alloc_aligned
	syscall 0x1a, os_heap_free
	syscall 0x1b, os_device_register
	syscall 0x1c, os_open
	syscall 0x1d, os_close
	syscall 0x1e, os_read
	syscall 0x1f, os_write
	syscall 0x20, os_seek
	syscall 0x21, os_ioctl
	syscall 0x22, os_ioctlv
	syscall 0x23, os_open_async
	syscall 0x24, os_close_async
	syscall 0x25, os_read_async
	syscall 0x26, os_write_async
	syscall 0x27, os_seek_async
	syscall 0x28, os_ioctl_async
	syscall 0x29, os_ioctlv_async
	syscall 0x2a, os_message_queue_ack
	syscall 0x34, os_software_IRQ
	syscall 0x3f, __os_sync_before_read
	syscall 0x40, __os_sync_after_write
	syscall 0x42, os_ios_boot
	syscall 0x46, os_check_DI_reset
	syscall 0x4c, os_kernel_set_version
	syscall 0x4d, os_kernel_get_version
	syscall 0x4f, os_virt_to_phys
	syscall 0x50, os_set_dvd_video_mode
	syscall 0x54, os_set_ahbprot


/*
 * ARM syscalls
 */
	.code 32
	.global svc_write
svc_write:
	adds	r1, r0, #0
	movs	r0, #4
	svc	0xab
	bx	lr
