/*
 * bootls.c
 *
 * Boot a Linkstation kernel of type firmimg.bin
 *
 * U-Boot loader code for Linkstation kernel. A file of type firmimg.bin
 * consists of a header, immediately followed by a compressed kernel image,
 * followed by a compressed initrd image.
 *
 * Copyright (C) 2006 Mihai Georgian <u-boot@linuxnotincluded.org.uk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 *
 * Derived from:
 *
 * arch/ppc/common/misc-simple.c (linux-2.4.17_mvl21-sandpoint)
 * Author: Matt Porter <mporter@mvista.com>
 * Derived from arch/ppc/boot/prep/misc.c
 * 2001 (c) MontaVista, Software, Inc.
 *
 * common/cmd_bootm.c (u-boot-1.1.4)
 * (C) Copyright 2000-2002
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 */

#include <common.h>
#include <command.h>

#include "firminfo.h"

#define _ALIGN(addr,size)       (((addr)+size-1)&(~(size-1)))

struct bi_record {
	unsigned long tag;		/* tag ID */
	unsigned long size;		/* size of record (in bytes) */
	unsigned long data[0];	/* data */
};

#define BI_FIRST	0x1010	/* first record - marker */
#define BI_LAST		0x1011	/* last record - marker */

extern int do_reset (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[]);
extern int gunzip(void *, int, unsigned char *, int *);

/*
 * output BYTE data
 */
static inline void outb(volatile unsigned char *addr, int val)
{
	asm volatile("eieio");
	asm volatile("stb%U0%X0 %1,%0; sync; isync" : "=m" (*addr) : "r" (val));
}

unsigned long checksum_check(unsigned char* addr, unsigned long size)
{
	long *laddr = (long *)addr;
	unsigned long sum = 0,remain = 0;
	int i;
	while(size>=4) {
		sum += *laddr;
		laddr++;
		size -= 4;
	}
	addr = (unsigned char*)laddr;
	for(i=0;i<4;++i) {
		remain = remain << 8;
		if(size>i) remain += *addr;
		addr++;
		}
	sum += remain;
	return sum;
}

void do_boot_lskernel (cmd_tbl_t *cmdtp,
					int flag,
					int argc,
					char *argv[],
					unsigned long load_addr,
					unsigned long *len_ptr,
					int verify)
{
	DECLARE_GLOBAL_DATA_PTR;

	char 			*zimage_start;
	int 			zimage_size;
	unsigned long 	initrd_start;
	unsigned long 	initrd_end;
	unsigned long 	sp;
	unsigned long 	cmd_start;
	unsigned long 	cmd_end;
	char 			*cmdline;
	char 			*s;
	bd_t 			*kbd;
	void    		(*kernel)(bd_t *, ulong, ulong, ulong, ulong);
	unsigned long	iflag;
	struct firminfo *info = (struct firminfo *)load_addr;
	struct bi_record *rec;

	int				i;
	char			*flashstr="FLASH";

	for (i=0; i <= 4; i++)
		if (info->subver[i] != flashstr[i]) {
			puts ("Not a Linkstation kernel\n");
			return;
		}

	printf("\n******* Product Information *******\n");
	printf("----------------------------------\n");

	printf("Product Name: %s\n", info->firmname);
	printf("         VER: %d.%02d\n", info->ver_major, info->ver_minor);
	printf("        Date: %d/%d/%d %d:%d:%d\n",
			info->year+1900, info->mon, info->day,
			info->hour,info->min,info->sec);
	printf("----------------------------------\n");
	
	if (verify) {
		printf("Verifying checksum... ");
		if (checksum_check((unsigned char*)info, info->size) != 0) {
			printf("Failed!: checksum %08X, expecting 0\n",
					checksum_check((unsigned char*)info, info->size));
			return; /* Returns on error */
		} else
			printf("OK\n");
	}

	zimage_start = (char*)info + info->kernel_offset;
	zimage_size  = (int)info->kernel_size;
	iflag = disable_interrupts();
	puts("Uncompressing kernel...");
	if (gunzip(0, 0x400000, zimage_start, &zimage_size) != 0) {
		puts ("Failed! MUST reset board to recover\n");
		do_reset (cmdtp, flag, argc, argv);
	} else
		puts("done.\n");

	/*
	 * Allocate space for command line and board info - the
	 * address should be as high as possible within the reach of
	 * the kernel (see CFG_BOOTMAPSZ settings), but in unused
	 * memory, which means far enough below the current stack
	 * pointer.
	 */

	asm( "mr %0,1": "=r"(sp) : );
	debug ("## Current stack ends at 0x%08lX ", sp);
	sp -= 2048;		/* just to be sure */
	if (sp > CFG_BOOTMAPSZ)
		sp = CFG_BOOTMAPSZ;
	sp &= ~0xF;
	debug ("=> set upper limit to 0x%08lX\n", sp);

	cmdline = (char *)((sp - CFG_BARGSIZE) & ~0xF);
	if ((s = getenv("bootargs")) == NULL)
		s = "root=/dev/hda1";
	strcpy (cmdline, s);
	cmd_start    = (ulong)&cmdline[0];
	cmd_end      = cmd_start + strlen(cmdline);
	debug ("## cmdline at 0x%08lX ... 0x%08lX\n", cmd_start, cmd_end);

	kbd = (bd_t *)(((ulong)cmdline - sizeof(bd_t)) & ~0xF);
	*kbd = *(gd->bd);
	if ((s = getenv ("clocks_in_mhz")) != NULL) {
		/* convert all clock information to MHz */
		kbd->bi_intfreq /= 1000000L;
		kbd->bi_busfreq /= 1000000L;
	}

	kernel = (void (*)(bd_t *, ulong, ulong, ulong, ulong))0x4;

	if (info->initrd_size > 0) {
		initrd_start = (unsigned long)((char*)info + info->initrd_offset);
		initrd_end   = initrd_start + info->initrd_size;
		if(initrd_start > 0xffc00000 && initrd_end < 0xffefffff) {
			unsigned long nsp;
			unsigned long data;

			data = initrd_start;
			/*
			 * the inital ramdisk does not need to be within
			 * CFG_BOOTMAPSZ as it is not accessed until after
			 * the mm system is initialised.
			 *
			 * do the stack bottom calculation again and see if
			 * the initrd will fit just below the monitor stack
			 * bottom without overwriting the area allocated
			 * above for command line args and board info.
			 */
			asm( "mr %0,1": "=r"(nsp) : );
			nsp -= 2048;			/* just to be sure */
			nsp &= ~0xF;
			nsp -= info->initrd_size;
			nsp &= ~(4096 - 1);		/* align on page */
			initrd_start = nsp;
			initrd_end = initrd_start + info->initrd_size;
			printf ("Loading Ramdisk at 0x%08lX, end 0x%08lX ... ",
					initrd_start, initrd_end);
			memmove ((void *)initrd_start, (void *)data, info->initrd_size);
			puts ("OK\n");
		}
	} else {
		initrd_start = 0;
		initrd_end   = 0;
	}

	/*
	 * The kernel looks for this structure even if
	 * the information in it is replaced by the
	 * Linkstation kernel
     */ 
	rec = (struct bi_record *)_ALIGN((unsigned long)zimage_size +
					(1 << 20) - 1,(1 << 20));
	rec->tag = BI_FIRST;
	rec->size = sizeof(struct bi_record);
	rec = (struct bi_record *)((unsigned long)rec + rec->size);
	rec->tag = BI_LAST;
	rec->size = sizeof(struct bi_record);

#if defined(CONFIG_HLAN) || defined(CONFIG_HGLAN) || defined(CONFIG_HTGL)
	// kernel load done.
	outb(0x80004500, 0x49); // send signal
	outb(0x80004500, 0x49); // send signal
	outb(0x80004500, 0x49); // send signal
	outb(0x80004500, 0x49); // send signal
#endif
#if defined(CONFIG_HGLAN)
	// full speed 
	udelay(10000);  /* 10 msec */
	outb(0x80004500, 0x5D); // send signal
	outb(0x80004500, 0x5D); // send signal
	outb(0x80004500, 0x5D); // send signal
	outb(0x80004500, 0x5D); // send signal
#endif
#if defined(CONFIG_HTGL)
	// LINK/ACT led controll
	outb(0x80004500, 0x61); // a
	outb(0x80004500, 0x61); // a
	outb(0x80004500, 0x39); // 9
	outb(0x80004500, 0x31); // 1
	outb(0x80004500, 0x39); // 9
	outb(0x80004500, 0x30); // 0
	outb(0x80004500, 0x92); // 1000Mbps down
	outb(0x80004500, 0x92); // 1000Mbps down

	udelay(10000);  /* 10 msec */
	outb(0x80004500, 0x61); // a
	outb(0x80004500, 0x61); // a
	outb(0x80004500, 0x39); // 9
	outb(0x80004500, 0x30); // 0
	outb(0x80004500, 0x39); // 9
	outb(0x80004500, 0x30); // 0
	outb(0x80004500, 0x90); // 100Mbps down
	outb(0x80004500, 0x90); // 100Mbps down

	udelay(10000);  /* 10 msec */
	outb(0x80004500, 0x61); // a
	outb(0x80004500, 0x61); // a
	outb(0x80004500, 0x38); // 8
	outb(0x80004500, 0x46); // F
	outb(0x80004500, 0x39); // 9
	outb(0x80004500, 0x30); // 0
	outb(0x80004500, 0x8E); // 10Mbps down
	outb(0x80004500, 0x8E); // 10Mbps down

	udelay(10000);  /* 10 msec */
	outb(0x80004500, 0x5F); // _
	outb(0x80004500, 0x5F); // _
#endif

/*
 * This is what the original boot loader sends
 * just before jumping to the kernel start
 */
	outb(0xFF000001, 0xFF);

	puts("Booting the kernel\n");

	/*
	 * Linux Kernel Parameters:
	 *   r3: ptr to board info data
	 *   r4: initrd_start or 0 if no initrd
	 *   r5: initrd_end - unused if r4 is 0
	 *   r6: Start of command line string
	 *   r7: End   of command line string
	*/
	(*kernel)((bd_t *)0xFF000001, initrd_start, initrd_end, cmd_start, cmd_end);
}

/* vim: set ts=4: */
