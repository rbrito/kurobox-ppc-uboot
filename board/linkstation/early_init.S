/*
 * board/linkstation/early_init.S
 *
 * Begin at some arbitrary location in RAM or Flash
 *  Initialize core registers
 *  Configure memory controller (Not executing from RAM)
 *  Initialize UARTs
 *  Simple RAM test (currently suspended)
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
 * Modified for U-Boot from arch/ppc/boot/linkstation/head.S from
 * the GPL code for the Buffalo Terastation, derived in its turn from:
 *
 * arch/ppc/boot/sandpoint/head.S
 *
 * Initial board bringup code for Motorola SPS Sandpoint test platform
 *
 * Author: Mark A. Greer
 *	   mgreer@mvista.com
 * Derived from arch/ppc/boot/pcore/head.S (mporter@mvista.com)
 *
 * Copyright 2001 MontaVista Software Inc.
 */

#include <config.h>
#include <ppc_asm.tmpl>
#include <mpc824x.h>
#include <ppc_defs.h>
#include <asm/cache.h> 

#if defined(CONFIG_LAN) || defined(CONFIG_HLAN)
#define RAM_SIZE        0x04000000
#elif defined(CONFIG_HGLAN) || defined(CONFIG_HTGL)
#define RAM_SIZE        0x08000000
#endif

#define UART1           0x80004500
#define UART1_IER       0x80004501
#define UART1_FCR       0x80004502
#define UART1_LCR       0x80004503
#define UART1_DCR       0x80004511
#define UART2           0x80004600
#define UART2_IER       0x80004601
#define UART2_FCR       0x80004602
#define UART2_LCR       0x80004603
#define UART2_DCR       0x80004611

#define WM32(address,data) \
	lis	r3, address@h; \
	ori	r3, r3, address@l; \
	lis	r4, data@h; \
	ori	r4, r4, data@l; \
	stw	r4, 0x0000(r3); \
	sync;  \
	isync;

#define WM16(address,data) \
	lis	r3, address@h; \
	ori	r3, r3, address@l; \
	li	r4, data; \
	sth	r4, 0x0000(r3); \
	sync;  \
	isync;

#define WM8(address,data) \
	lis	r3, address@h; \
	ori	r3, r3, address@l; \
	li	r4, data; \
	stb	r4, 0(r3); \
	sync;  \
	isync;

	.text

	.globl	early_init_f
early_init_f:
/*
 * Configure core registers
 */

	/* Establish default MSR value, exception prefix 0xFFF */
	li	r3,MSR_IP|MSR_FP
	mtmsr	r3

	/* Clear BATS */
	li      r8,0
	mtspr   DBAT0U,r8
	mtspr	DBAT0L,r8
	mtspr   DBAT1U,r8
	mtspr	DBAT1L,r8
	mtspr   DBAT2U,r8
	mtspr	DBAT2L,r8
	mtspr   DBAT3U,r8
	mtspr	DBAT3L,r8
	mtspr   IBAT0U,r8
	mtspr	IBAT0L,r8
	mtspr   IBAT1U,r8
	mtspr	IBAT1L,r8
	mtspr	IBAT2U,r8
	mtspr   IBAT2L,r8
	mtspr   IBAT3U,r8
	mtspr	IBAT3L,r8
	isync
	sync
	sync

	/* Set segment registers */
	lis	r8, 0x0000
	isync
	mtsr	SR0,r8
	mtsr	SR1,r8
	mtsr	SR2,r8
	mtsr	SR3,r8
	mtsr	SR4,r8
	mtsr	SR5,r8
	mtsr	SR6,r8
	mtsr	SR7,r8
	mtsr	SR8,r8
	mtsr	SR9,r8
	mtsr	SR10,r8
	mtsr	SR11,r8
	mtsr	SR12,r8
	mtsr	SR13,r8
	mtsr	SR14,r8
	mtsr	SR15,r8
	isync
	sync
	sync

	/* Disable L1 icache/dcache */
	li	r4,0x0000
	isync
	mtspr	HID0,r4
	sync
	isync
	
	/* Flash Invalidate L1 icache/dcache */
	
	ori	r4,r4,0x8000
	ori	r8,r4,0x0800
	isync
	mtspr	HID0,r8
	sync
	isync
	
	/* Older cores need to manually clear ICFI bit */
	
	mtspr	HID0,r4
	sync
	isync

#if !defined(CFG_RAMBOOT)
melco_config_start:
	/* --- CPU Configration registor setting for LinkStation --- */
	WM32(0x80041020,0x000000a0) /* Reset EPIC */

	/* errata for latency timer */
	WM32(0xFEC00000,0x0d000080)
	WM8(0xFEE00001,0x20)
	/* cash size */
	WM32(0xFEC00000,0x0c000080)
	WM8(0xFEE00000,0x08)
	/*  PCI configuration command register */
	WM32(0xFEC00000,0x04000080)
	WM16(0xFEE00000,0x0600)
	/* Processor interface configuration register 1 */
	WM32(0xFEC00000,0xa8000080)
	/* WM32(0xFEE00000,0xd8131400) */
	lis	r3, 0xFEE00000@h 
	ori	r3, r3, 0xFEE00000@l
	
	lwz r5, 0(r3)  /* load PCIR1 Config */
	lis r4, 0x0
	ori r4, r4, 0x1000
	and r5, r4, r5  /* Get Bit20(RCS0) */
		
	lis	r4, 0xd8130400@h
	ori	r4, r4, 0xd8130400@l
	or  r4, r4, r5   /* Save (RCS0) */
	
	stw	r4, 0x0000(r3)
	sync
	isync
	
	/* Processor interface configuration register 2 */
	WM32(0xFEC00000,0xac000080)
	WM32(0xFEE00000,0x00000004)
	/* Embeded Utility Memory Block Base Address register */
	WM32(0xFEC00000,0x78000080)
	WM32(0xFEE00000,0x00000080)
	/* Address map B option register */
	WM32(0xFEC00000,0xe0000080)
	WM8(0xFEE00000,0x20) /* DLL_RESET on */
	
	/* Address map B option register */
	WM32(0xFEC00000,0xe0000080)
	WM8(0xFEE00000,0xc0)
	/* PCI arbiter control register */
	WM32(0xFEC00000,0x46000080)
	WM16(0xFEE00002,0x00c0)

	/* Added to use the high drive strength for the memory selects & addressing */
	WM32(0xFEC00000,0x73000080)
		/* WM8(0xFEE00003,0x15) */  /*0x17*/
		/* Motorola Errata refer to User's Manual Errata#19 */
		/* WM8(0xFEE00003,0xD5) */
	WM8(0xFEE00003,0x95)

	/* set miscellaneous I/O control register 1 */
	WM32(0xFEC00000,0x76000080)
	WM8(0xFEE00002,0x00) /*0x02*/
	/* set miscellaneous I/O control register 2 */
	WM32(0xFEC00000,0x77000080)
	WM8(0xFEE00003,0x30)  /* 0x30 */

	/* init memory controller */
	WM32(0xFEC00000,0x80000080)
	WM32(0xFEE00000,0x00FFFFFF)

	WM32(0xFEC00000,0x84000080)
	WM32(0xFEE00000,0xFFFFFFFF)
	
	WM32(0xFEC00000,0x90000080)
#if defined(CONFIG_LAN) || defined(CONFIG_HLAN)
	WM32(0xFEE00000,0x3FFFFFFF) /* 64MB */
#elif defined(CONFIG_HGLAN) || defined(CONFIG_HTGL)
	WM32(0xFEE00000,0x7FFFFFFF) /* 128MB */
#endif

	WM32(0xFEC00000,0x94000080)
	WM32(0xFEE00000,0xFFFFFFFF)
	
	WM32(0xFEC00000,0x88000080)
	WM32(0xFEE00000,0x00030303)
	/* EMSAR2 */
	WM32(0xFEC00000,0x8C000080)
	WM32(0xFEE00000,0x03030303)
	/* select EMSER1 */
	WM32(0xFEC00000,0x98000080)
	WM32(0xFEE00000,0x00030303)
	/* select EMSER2 */
	WM32(0xFEC00000,0x9C000080)
	WM32(0xFEE00000,0x03030303)

	/* MMCR1 */
	WM32(0xFEC00000,0xf0000080)
#if defined(CONFIG_LAN) || defined(CONFIG_HLAN)
	WM32(0xFEE00000,0x0200E005)	/* bank 0 13xnx4 */
#elif defined(CONFIG_HGLAN) || defined(CONFIG_HTGL)
	WM32(0xFEE00000,0x0200E005)	/* bank 0 13xnx4 */
#endif
	/* MCCR2 */
	WM32(0xFEC00000,0xf4000080)
#if defined(CONFIG_LAN) || defined(CONFIG_HLAN)
	WM32(0xFEE00000,0xe0150000) /* 100MHz Memory bus */
#elif defined(CONFIG_HGLAN) || defined(CONFIG_HTGL)
	WM32(0xFEE00000,0x80150000) /* 133MHz Memory bus */
#endif
	/* MCCR3 */
	WM32(0xFEC00000,0xf8000080)	
	WM32(0xFEE00000,0x00000077)	/* BSTOPRE_M =7 / REFREC=8 */

	/* MCCR4 */
	WM32(0xFEC00000,0xfc000080)
#if defined(CONFIG_LAN) || defined(CONFIG_HLAN)
	WM32(0xFEE00000,0x29233222)	/* CAS latency=2, burst length=8, Ext Rom=eable */
#elif defined(CONFIG_HGLAN) || defined(CONFIG_HTGL)
	WM32(0xFEE00000,0x29323222)	/* CAS latency=3, burst length=4, Ext Rom=eable */
#endif

	/* Output driver control register */
	WM32(0xFEC00000,0x73000080)
	WM8(0xFEE00003,0x15)			/*  for all 40 ohm */
	/* CLK driver Control Register */
	WM32(0xFEC00000,0x74000080)
	WM16(0xFEE00000,0x7078)
	/* select MBEN */
	WM32(0xFEC00000,0xa0000080)
	WM8(0xFEE00000, 0x01)
	/* MPM */
	WM32(0xFEC00000,0xa3000080)
#if defined(CONFIG_LAN) || defined(CONFIG_HLAN)
	WM8(0xFEE00003,0xF2)	/* PGMAX = 242 */
#elif defined(CONFIG_HGLAN) || defined(CONFIG_HTGL)
	WM8(0xFEE00003,0xC9)	/* PGMAX = 201 */
#endif
	/* ERCR s */
	WM32(0xFEC00000,0xd0000080) /*	; select ERCR1 */
	WM32(0xFEE00000,0xffffff85)
	WM32(0xFEC00000,0xd4000080) /*	; select ERCR2 */
	WM32(0xFEE00000,0xffffff05)
	WM32(0xFEC00000,0xd8000080) /*	; select ERCR3 */
	WM32(0xFEE00000,0x0000f80f)
	WM32(0xFEC00000,0xdc000080) /*  ; select ERCR4 */
	WM32(0xFEE00000,0x0e000000)

	/* MCCR1 */
	WM32(0xFEC00000,0xf0000080)
	WM32(0xFEE00000,0x0200E805)  /* 11 + 3 clock wait MEMGO on */

	/* Init UART for AVR */
	WM8(UART1_LCR,0x00) /* clear LCR */
	WM8(UART1_IER,0x00) /* disable interrupt */
	WM8(UART1_LCR,0x80) /* set LCR[DLAB] bit */
	WM8(UART1_DCR,0x01) /* set DUART mode */
#if defined(CONFIG_LAN) || defined(CONFIG_HLAN)
	WM8(UART1,    0x8B) /* set DLL(baudrate 9600bps, 100MHz) */
	WM8(UART1_IER,0x02) /* set DLM(baudrate 9600bps, 100MHz) */
#elif defined(CONFIG_HGLAN) || defined(CONFIG_HTGL)
	WM8(UART1,    0x61) /* set DLL(baudrate 9600bps, 133MHz) */
	WM8(UART1_IER,0x03) /* set DLM(baudrate 9600bps, 133MHz) */
#endif
	WM8(UART1_LCR,0x1b) /* set 8data, 1stop, even parity */
	WM8(UART1,    0x00) /* clear MCR */
	WM8(UART1_FCR,0x07) /* clear & enable FIFO */

	/* Init UART for CONSOLE */
	WM8(UART2_LCR,0x00) /* clear LCR */
	WM8(UART2_IER,0x00) /* disable interrupt */
	WM8(UART2_LCR,0x80) /* set LCR[DLAB] bit */
	WM8(UART1_DCR,0x01) /* set DUART mode */
#if defined(CONFIG_LAN) || defined(CONFIG_HLAN)
	WM8(UART2,    0x6C) /* set DLL(baudrate 57600bps, 100MHz) */
	WM8(UART2_IER,0x00) /* set DLM(baudrate 57600bps, 100MHz) */
#elif defined(CONFIG_HGLAN) || defined(CONFIG_HTGL)
	WM8(UART2,    0x90) /* set DLL(baudrate 57600bps, 133MHz) */
	WM8(UART2_IER,0x00) /* set DLM(baudrate 57600bps, 133MHz) */
#endif
	WM8(UART2_LCR,0x03) /* set 8data, 1stop, non parity */
	WM8(UART2,    0x00) /* clear MCR */
	WM8(UART2_FCR,0x07) /* clear & enable FIFO */
#endif /* !defined (CFG_RAMBOOT)

	/* PCI Command Register initialize */
	lis r3, 0x8000
	ori r3, r3, 0x0004
	lis r4, 0xFEC0
	ori r4, r4, 0x0000
	stwbrx r4, 0, r3
	sync
	isync

	li r6, 0x0006
	lis r5, 0xFEE0
	ori r5, r5, 0x0000
	sthbrx r5, 0, r6
	sync
	isync

#if !defined(CFG_RAMBOOT)
check_ram:
	/* Wait 1 sec  for AVR become enable */
	li	r3,1000
	mulli	r3,r3,1000
	mulli	r4,r3,1000	/* nanoseconds */
	addi	r4,r4,39
	li	r5,40		/* 40ns if for 100 Mhz bus */
	divw	r4,r4,r5	/* BUS ticks */
1:	mftbu	r5
	mftb	r6
	mftbu	r7
	cmp	0,r5,r7
	bne	1b		/* Get [synced] base time */
	addc	r9,r6,r4	/* Compute end time */
	addze	r8,r5
2:	mftbu	r5
	cmp	0,r5,r8
	blt	2b
	bgt	3f
	mftb	r6
	cmp	0,r6,r9
	blt	2b
#if 1
3:
#else
	/* Check RAM */
	/* set start address(0x00000000) */
3:	xor r4,r4,r4
	lis r5,     RAM_SIZE@h
	ori r5, r5, RAM_SIZE@l
	lis r6,     0xaaaa  /* mask pattern a */
	ori r6, r6, 0xaaaa
	lis r7,     0x5555  /* mask pattern b */
	ori r7, r7, 0x5555
	lis r8,     0x0000  /* check step size */
	ori r8, r8, 0x0100
check_ram_loop:	
	cmp 0,r4,r5
	beq  check_ram_end
	stw r6,0(r4)
	isync
	lwz r3,0(r4)
	isync
	cmp 0,r3,r6
	bne ram_error
	stw r7,0x00fc(r4)
	isync
	lwz r3,0x00fc(r4)
	isync
	cmp 0,r3,r7
	bne ram_error
	add r4,r4,r8
	b   check_ram_loop
ram_error:
#if defined(CONFIG_LAN)
	WM8(UART1,0x39)  /* ram error */
#elif defined(CONFIG_HGLAN) ||defined(CONFIG_HLAN) || defined(CONFIG_HTGL)
	WM8(UART1,0x6F)  /* ram error */
#endif
	b   ram_error
check_ram_end:
#endif /* #if 1 */
#endif /* !defined (CFG_RAMBOOT) */

/* The instruction cache is enabled and the data cache is disabled */
	blr
