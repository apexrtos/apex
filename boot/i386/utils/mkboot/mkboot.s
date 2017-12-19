#
# Copyright (c) 2005, Kohsuke Ohtani
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. Neither the name of the author nor the names of any co-contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

#
# mkboot.S - DOS utility to install boot sector into disk
#
# Usage: mkboot [drive]
#  ex. mkboot a:
#
# Read image file "bootsect.bin" and write it to disk.
# Current boot sector is automatically saved as file "bootsect.dbg".
#
# Prerequisite:	Correct BIOS Parameter Block (BPB) has already stored
# in the target disk device. Generally, this data is written by a format tool.
#
.code16
.text
.align 1

#
# Entry point
#
main:
	movw	%cs, %ax
	movw	%ax, %ds
	movw	%ax, %es
	cld

	call	get_drive			# Get drive information
	jc	dos_exit

	call	read_file			# Read boot sector file
	jc	dos_exit

	call	read_bsec			# Read actual boot sector
	jc	dos_exit

	call	store_bpb			# Copy BPB data
	jc	dos_exit

#	 cmp	 $1, dbg_mode
#	 je	 skip_write

	call	write_bsec			# Write boot sector
	jc	dos_exit
skip_write:

	call	debug_file			# Put to file for debug
	movw	$msg_ok, %dx
	call	puts

dos_exit:
	movb	$0x4c, %ah			# Return to DOS
	int	$0x21
	ret

#
# Get specified drive
#
get_drive:
	pushw	%es
	pushaw

	movb	$0x62, %ah			# Get DOS PSP
	int	$0x21

	movw	%bx, %es
	movb	%es:(0x80), %al			# Get parameter size
	cmpb	$0, %al				# No parameter ?
	je	invalid_arg

	movb	%es:(0x82), %al			# Get drive character
	cmpb	$0x41, %al			# A ?
	jb	invalid_arg
	cmpb	$0x5a, %al			# Z ?
	ja	chk_low
	subb	$0x41, %al			# - A
	jmp	drive_ok

chk_low:
	cmpb	$0x61, %al			# a ?
	jb	invalid_arg
	cmpb	$0x7a, %al			# z ?
	ja	invalid_arg
	subb	$0x61, %al			# - a

drive_ok:
	cmpb	$2, %al
	jb	skip_verify

	movw	$err_dbg, %dx
	pushw	%ax
	call	puts
	movb	$1, dbg_mode
	popw	%ax

skip_verify:
	movb	%al, drive

	movb	$0, phys_drive
	cmp	$2, %al
	jb	not_hdd
	movb	$0x80, phys_drive
not_hdd:
	popaw
	popw	%es
	clc
	ret

invalid_arg:
	popaw
	popw	%es
	movw	$msg_usage, %dx
	call	puts
	stc
	ret

#
# puts
#
puts:
	movb	$0x09, %ah
	int	$0x21
	ret

#
# Read file
#
read_file:
	pushaw
	movw	$bsec_name, %dx
	movb	$0x3d, %ah			# Open file
	movb	$0x80, %al
	int	$0x21
	jc	error_read

	movw	%ax, %bx
	movw	$tmpSect, %dx
	movw	512, %cx
	movb	$0x3f, %ah			# Read file
	int	$0x21
	jc	error_read

	mov	$0x3e, %ah			# Close file
	int	$0x21

	movw	$tmpSect, %bx
	movw	510(%bx), %ax
	cmpw	$0xaa55, %ax
	jne	error_file
	popaw
	clc
	ret

error_file:
	movw	$err_bad_file, %dx
	call	puts
	popaw
	stc
	ret

error_read:
	movw	$err_not_found, %dx
	call	puts
	popaw
	stc
	ret

#
# Read boot sector image from disk
#
read_bsec:
	pushaw
	movw	5, %cx
retry_loop:
	pushw	%cx

	movw	$dos_packet, %bx		# bx = buffer address
	movw	$orgSect, %ax
	movw	%ax, 6(%bx)
	movw	%cs, %ax
	movw	%ax, 8(%bx)
	movl	$0, (%bx)			# Sector = 0
	movw	$1, 4(%bx)			# Num of sector = 1
	movb	drive, %al			# al = drive number
	movw	$0xffff, %cx			# Extended call
	int	$0x25
	popw	%ax

	popw	%cx
	jnc	read_ok
	loopw	retry_loop

	movw	$err_read, %dx
	call	puts
	stc
read_ok:
	popaw
	ret


#
# Write boot sector image to disk
#
write_bsec:
	pushaw
	movw	$dos_packet, %bx		# bx = buffer address
	movw	$tmpSect, %ax
	movw	%ax, 6(%bx)
	movw	%cs, %ax
	movw	%ax, 8(%bx)
	movl	$0, (%bx)			# Sector = 0
	movw	$1, 4(%bx)			# Num of sector = 1
	movb	drive, %al			# al = drive number
	movw	$0xffff, %cx			# Extended call
	int	$0x26
	popw	%ax

	jnc	write_ok

	movw	$err_write, %dx
	call	puts
	popaw
	stc
write_ok:
	popaw
	ret

#
# Store BPB from current disk BPB
#
store_bpb:
	cld
	movw	$orgSect, %si
	addw	$3, %si				# Skip jump instruction
	addw	$8, %si				# Skip OEM ID
	movw	$tmpSect, %di
	addw	$3, %di
	addw	$8, %di
	movw	$0x33, %cx			# Copy disk parameter
	rep
	movsb

###	movw	$tmpSect, %di			# Update physical drive
###	addw	$0x24, %di
###	movb	phys_drive, %al
###	movb	%al, (%di)

	clc
	ret

#
# Create new BPB file for debug
#
debug_file:
	movw	$dbg_name, %dx
	movb	$0x3c, %ah			# Create file
	movw	$0, %cx
	int	$0x21
	jc	exit_debug

	movw	%ax, %bx
	movw	$tmpSect, %dx
	movw	$512, %cx
	movb	$0x40, %ah			# Read file
	int	$0x21
	jc	exit_debug

	movb	$0x3e, %ah			# Close file
	int	$0x21

exit_debug:
	ret

#
# Data
#
drive:		.byte	0
phys_drive:	.byte	0
dbg_mode:	.byte	0

.align 4
dos_packet:
		.long	0			# Sector
		.word	1			# Number of sector
		.word	0			# Buffer offset
		.word	0			# Buffer segment

bsec_name:	.asciz	"bootsect.bin"
dbg_name:	.asciz	"bootsect.dbg"

msg_usage:	.ascii	"mkboot [drive]"
		.byte	0xd,0xa,0x24
msg_ok:		.ascii	"System transferred"
		.byte	0xd,0xa,0x24
err_dbg:	.ascii	"Warning: Write to HDD!"
		.byte	0xd,0xa,0x24
err_not_found:	.ascii	"Error: bootsect.bin was not found"
		.byte	0xd,0xa,0x24
err_bad_file:	.ascii	"Error: bootsect.bin is not valid"
		.byte	0xd,0xa,0x24
err_read:	.ascii	"Error: Disk read errror"
		.byte	0xd,0xa,0x24
err_write:	.ascii	"Error: Disk write errror"
		.byte	0xd,0xa,0x24

.align 16
orgSect:	.fill	512,1,0
tmpSect:	.fill	512,1,0

