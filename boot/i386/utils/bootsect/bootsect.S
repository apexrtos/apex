/*-
 * Copyright (c) 2005, Kohsuke Ohtani
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/**
 * bootsect.s - Boot sector for FAT
 *
 * The boot sector is 512 byte code to load the OS image. It is loaded
 * to address 0:7c00 by POST BIOS.
 * The boot sector searches the target file within the root directory of
 * FAT file system, and loads it to predefined memory address. Then, it
 * jumps to the first byte of the loaded image.
 *
 * All disk access are done by using BIOS Int13h interface. The BIOS
 * parameter block (BPB) has the disk/FAT information, and it exists
 * in the first portion of the FAT boot sector. It must be filled by the
 * FAT format utility, or the prex kernel install utility (mkboot.com).
 * This program assumes that correct BPB is stored in the boot sector.
 *
 * Limitation:
 *  - Support only FAT12/16. FAT32 is not supported.
 *
 * Memory usage:
 *  > 5000 - 6FFF ... Disk work area
 *  > 7000 - 7BFF ... Stack
 *  > 7C00 - 7DFF ... This boot sector
 *  >10000 -      ... FAT cache
 *  >20000 -      ... Data cache
 *  >30000 -      ... Image load address
 */
.code16
.text
.align 1

# Memory locations
#define BOOT_STACK		0x7c00

#define LOAD_ADDR		0x30000
#define ENTRY_SEG		0x3000
#define ENTRY_OFF		0x0000

#define WORK_AREA		0x5000
#define FAT_SEG			0x1000
#define DATA_SEG		0x2000

#define LOAD_MAX		0xA0000

# FAT Directory entry
#define F_NAME			0
#define F_ATTR			11
#define F_RESERVED		12
#define F_TIME			22
#define F_DATA			24
#define F_CLUSTER		26
#define F_SIZE			28

#define DIR_SIZE		32
#define DIRENT_PER_SECTOR	16

# BIOS parameter block (BPB) location (%bp points to 0x7c00)
#define OEM_ID			0x03(%bp)
#define BYTE_PER_SECTOR		0x0b(%bp)
#define SECT_PER_CLUSTER	0x0d(%bp)
#define RESERVED_SECTORS	0x0e(%bp)
#define NUM_OF_FATS		0x10(%bp)
#define ROOT_ENTRIES		0x11(%bp)
#define TOTAL_SECTORS		0x13(%bp)
#define MEDIA_DESCRIPTOR	0x15(%bp)
#define SECTORS_PER_FAT		0x16(%bp)
#define SECTORS_PER_TRACK	0x18(%bp)
#define HEADS			0x1a(%bp)
#define HIDDEN_SECTORS		0x1c(%bp)
#define BIG_TOTAL_SECTORS	0x20(%bp)
#define PHYSICAL_DRIVE		0x24(%bp)
#define EXT_BOOT_SIGNATURE	0x26(%bp)
#define SERIAL_NO		0x27(%bp)
#define VOLUME_ID		0x2b(%bp)
#define FILE_SYS_ID		0x36(%bp)

#define FILE_SYS_ID_NUM		0x3a(%bp)

# Local data area (Note: These data will overlap the existing code)
#define FAT_START		0x40(%bp)
#define DATA_START		0x44(%bp)

.global _boot

#
# Boot the system
#
_boot:
	jmp	start				# Skip BPB
	nop					# Nop is for DOS compatibility

#
# BPB
#
	.ascii	"PREX1.00"
.fill 0x33, 1, 0				# Drive parameter must be
						# filled by intaller

#
# Setup stack and segment registers
#
start:
	cli
	cld					# Clear direction flag
	xorl	%eax, %eax			# Set EAX to zero
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %ss
	movw	$(BOOT_STACK), %sp
	movw	%sp, %bp			# EBP = Bios Parameter Block
	sti

#
# Display boot message
#
	movw	$load_msg, %si
	movw	$21, %cx
	call	puts

#
# Store disk information
#
	movl	HIDDEN_SECTORS, %ebx		# Get hidden sector
	movw	RESERVED_SECTORS, %ax		# Add reserved sector
	addl	%eax, %ebx			# High 16 bit of EAX is 0
	movl	%ebx, FAT_START			# FAT start = hidden + reserved

	movzbw	NUM_OF_FATS, %ax		# Normally 2
	mulw	SECTORS_PER_FAT			# AX = Num of sector of FATs
	addl	%ebx, %eax			# EAX = Start of root directory

	movw	ROOT_ENTRIES, %bx
	shrw	$4, %bx				# / 16 = DIRENT_PER_SECTOR
	movw	%bx, %cx			# CX = Num of sectors for root directory

	addl	%eax, %ebx			# DATA start = FAT start + root
	movl	%ebx, DATA_START		# Start sector of data area

#
# Find the OS image in the root directory
#
						# EAX = Start sector of root
next_sector:
	pushw	%cx
	movl	$(WORK_AREA), %ebx
	pushw	%bx
	call	read_sector			# Read 1 sector in root
	popw	%di				# DI = dir_entry
	movw	$(DIRENT_PER_SECTOR), %cx	# CX = directory count
next_entry:
	cmpb	$0, (%di)			# End of dir entry ?
	je	error				# Not found
	testb	$0x18, F_ATTR(%di)		# Subdir or Volume ?
	jnz	not_file			# Skip it
	pusha
	movw	$11, %cx			# File name + ext = 11 byte
	movw	$image_name, %si
	repe					# Compare file name
	cmpsb
	popa
	je	found_file
not_file:
	addw	$(DIR_SIZE), %di		# Check next directory entry
	loop	next_entry
	popw	%cx
	loop	next_sector
						# Fall through
#
# Error case
#
error:
	movw	$err_msg, %si
	movw	$5, %cx
	call	puts
hang:
	hlt
	jmp	hang				# Stop here

#
# Load image
#
found_file:
	movzwl	F_CLUSTER(%di), %eax		# EAX = 1st cluster of loader
	movl	$(LOAD_ADDR), %ebx		# EBX = 32bit load address
load_next:
	call	read_cluster			# Read cluster of loader
	call	next_cluster			# Get next cluster# in EAX
	jb	load_next			# EOF ?

#
# Turn fdd motor off
#
	movw	$0x3f2, %dx
	xorb	%al, %al
	outb	%al, %dx

#
# Jump to loaded image
#
	ljmp	$0x3000, $0x0

#
# Puts - Print string
#
# Entry:
#   SI - Pointer to message string
#   CX - Number of character
#
puts:
	lodsb
	movb	$0x0e, %ah
	movw	$0x0007, %bx
	int	$0x10
	loop	puts
	ret

#
# next_cluster - Return next cluster
#
# Entry:
#   EAX - Current cluter#
#
# Exit:
#   AF - End of cluster
#   EAX - Next cluter#
#
# Modified:
#   Flags,CX,EDX,SI,DI
#
next_cluster:
	pushl	%ebx
	movw	%ax, %di			# Save cluster# in DI

	movw	$0xfff8, %si			# Set default EOF to FAT16

	movl	%eax, %ecx
	shll	$1, %eax			# * 2

	cmpb	$0x36, FILE_SYS_ID_NUM		# ID is 'FAT16' ?
	je	fat_16
	addl	%ecx, %eax			# * 3
	shrl	$1, %eax			# / 2
	movw	$0xff8, %si			# EOF for FAT12
fat_16:
						# EAX - Offset of FAT entry
	xorw	%dx, %dx
	divw	BYTE_PER_SECTOR
	addl	FAT_START, %eax			# EAX = Sector# for FAT
						# DX = Offset in sector
	movl	$(WORK_AREA), %ebx
	pushw	%bx
	call	read_sector			# Read 2 sector for border
	call	read_sector			# data
	popw	%bx
	addw	%dx, %bx
	movw	(%bx), %ax
	cmpw	$0xfff8, %si			# FAT16 ?
	je	chk_end

	shrw	$1, %di
	jc	odd_pos
	andb	$0x0f, %ah
	jmp	chk_end
odd_pos:
	shrw	$4, %ax
chk_end:
	cmpw	%si, %ax
	popl	%ebx
	ret

#
# read_cluster - Read one cluster
#
# Entry:
#   EBX - 32-bit pointer to buffer
#   EAX - Cluster number
#
# Exit:
#   EBX - Point to next buffer
#
# Modified:
#   flags,ECX,ECX,EDX
#
read_cluster:
	pushl	%eax
	decw	%ax				# Translate clust# to sec#
	decw	%ax
	xorl	%ecx, %ecx
	movb	SECT_PER_CLUSTER, %cl
	mull	%ecx
	addl	DATA_START, %eax		# EAX = Read sec#
						# CX = Read sector size
read_loop:
	call	read_sector
	cmpl	$(LOAD_MAX), %ebx
	jae	error
	loop	read_loop
	popl	%eax
	ret

#
# read_sector - Read one sector
#
# Entry:
#   EBX - 32-bit pointer to buffer
#   EAX - Logical sector# to read
#
# Exit:
#   EBX - Pointer to next buffer
#   EAX - Next sector
#
# Modified:
#   Flags
#
read_sector:
	pushal
	pushw	%ds
	pushw	%es

	movl	%eax, %esi			# ESI = buffer

	movzwl	SECTORS_PER_TRACK, %ecx		# Get sec/track
	xorl	%edx, %edx
	divl	%ecx				# EAX = track#
						# DX = sec#
	movw	$(DATA_SEG), %cx		# Check in cache
	leaw	last_data, %di
	cmpl	DATA_START, %esi
	jae	data_reqest
	movw	$(FAT_SEG), %cx
	leaw	last_fat, %di
data_reqest:
						# CX = Cached segment
	pushal					# [DI] = Cached track
	movw	%cx, %es
	xorw	%bx, %bx			# ES:BX = Cache address
	cmpl	(%di), %eax			# Last track ?
	je	hit_cache
	movl	%eax, (%di)			# Save current track#
	call	read_track
hit_cache:
	popal

	pushw	%es
	popw	%ds

	shlw	$9, %dx				# sec# * 512
	movw	%dx, %si			# DS:SI = Offset in cache

	movw	%bx, %di			# [EBX] -> ES:[DI]
	andw	$0xf, %di
	shrl	$4, %ebx
	movw	%bx, %es

	mov	$512, %cx			# Copy 1 sector
	rep
	movsb

	popw	%es
	popw	%ds
	popal

	addl	$512, %ebx			# Next buffer
	incl	%eax				# Next sector
	ret

#
# read_track - Read one track
#
# Entry:
#   ES:[BX] - Pointer to buffer
#   EAX	    - Track number to read
#
# Exit:
#   None
#
# Modified:
#   Flags,EAX,ECX,EDX
#
read_track:
	movzwl	HEADS, %ecx			# Get num of head
	xorl	%edx, %edx
	divl	%ecx				# AX = cyl#
						# DL = head#

	movb	%al, %ch			# CH = cyl# (low 8 bits)
	andb	$3, %ah
	shlb	$6, %ah
	orb	$1, %ah
	movb	%ah, %cl			# CL[7:6] = cyl# (high 2 bits)
						# CL[5:0] = sec# = 1
	movb	%dl, %dh			# DH = Head#
	movw	SECTORS_PER_TRACK, %ax		# AL = Num of sectors to read
	movb	$2, %ah				# AH = 02h (Read Disk Sectors)
	movb	PHYSICAL_DRIVE, %dl		# DL = Drive#
	int	$0x13				# Invoke Disk BIOS
	jc	error
	ret

#
# Local Data
#
last_fat:	.long	0xffffffff
last_data:	.long	0xffffffff

load_msg:	.ascii	"Loading "
image_name:	.ascii	"PREXOS     "
crlf:		.byte	0x0a, 0x0d
err_msg:	.ascii	"Error"

.org	510
		.word	0xaa55
