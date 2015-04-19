/*
 * hwctl.c
 *
 * LinkStation HW Control Driver
 *
 * Copyright (C) 2001-2004  BUFFALO INC.
 *
 * This software may be used and distributed according to the terms of
 * the GNU General Public License (GPL), incorporated herein by reference.
 * Drivers based on or derived from this code fall under the GPL and must
 * retain the authorship, copyright and license notice.  This file is not
 * a complete program and may only be used when the entire operating
 * system is licensed under the GPL.
 *
 */

#include <config.h>
#include <common.h>
#include <command.h>

#define mdelay(n)	udelay((n)*1000)

#define AVR_PORT CFG_NS16550_COM2
extern void udelay(unsigned long usec);


// output BYTE data
static inline void out_b(volatile unsigned char *addr, int val)
{
	__asm__ __volatile__("stb%U0%X0 %1,%0; eieio" : "=m" (*addr) : "r" (val));
}

#if 0
// PWR,DISK_FULL/STATUS,DIAG LED controll
void blink_led(unsigned char state)
{
#ifdef CONFIG_HTGL
	switch (state)
	{
		case FLASH_CLEAR_START:
		case FLASH_UPDATE_START:
			out_b(AVR_PORT, 0x61);
			out_b(AVR_PORT, 0x61);
			out_b(AVR_PORT, 0x38);
			out_b(AVR_PORT, 0x30);
			out_b(AVR_PORT, 0x34);
			out_b(AVR_PORT, 0x31);
			mdelay(10);
			out_b(AVR_PORT, 0x61);
			out_b(AVR_PORT, 0x61);
			out_b(AVR_PORT, 0x38);
			out_b(AVR_PORT, 0x31);
			out_b(AVR_PORT, 0x34);
			out_b(AVR_PORT, 0x31);
			mdelay(10);
			out_b(AVR_PORT, 0x71);
			out_b(AVR_PORT, 0x71);
//			out_b(AVR_PORT, 0x71);
//			out_b(AVR_PORT, 0x71);
			mdelay(10);
			out_b(AVR_PORT, 0x73);
			out_b(AVR_PORT, 0x73);
//			out_b(AVR_PORT, 0x73);
//			out_b(AVR_PORT, 0x73);
			mdelay(10);
			break;
		case FLASH_CLEAR_END:
		case FLASH_UPDATE_END:
			out_b(AVR_PORT, 0x70);
			out_b(AVR_PORT, 0x70);
//			out_b(AVR_PORT, 0x70);
//			out_b(AVR_PORT, 0x70);
			mdelay(10);
			out_b(AVR_PORT, 0x72);
			out_b(AVR_PORT, 0x72);
//			out_b(AVR_PORT, 0x72);
//			out_b(AVR_PORT, 0x72);
			mdelay(10);
			break;
		case RAID_RESYNC_START:
			break;
		case RAID_RESYNC_END:
			break;
		default:
			out_b(AVR_PORT, state);
			out_b(AVR_PORT, state);
			out_b(AVR_PORT, state);
			out_b(AVR_PORT, state);
			break;
	}
#else
	out_b(AVR_PORT, state);
	out_b(AVR_PORT, state);
	out_b(AVR_PORT, state);
	out_b(AVR_PORT, state);
#endif

}
#endif

// 2005.5.10 BUFFALO add
//--------------------------------------------------------------
static inline void miconCntl_SendUart(unsigned char dat)
{
	out_b((char *)AVR_PORT, dat);
	udelay(1000);
}

//--------------------------------------------------------------
void miconCntl_SendCmd(unsigned char dat)
{
	int i;
	
	for (i=0; i<4; i++){
		miconCntl_SendUart(dat);
	}
}

//--------------------------------------------------------------
void miconCntl_FanLow(void)
{
	debug("%s\n",__FUNCTION__);
#ifdef CONFIG_HTGL
	miconCntl_SendCmd(0x5C);
#endif
}
//--------------------------------------------------------------
void miconCntl_FanHigh(void)
{
	debug("%s\n",__FUNCTION__);
#ifdef CONFIG_HTGL
	miconCntl_SendCmd(0x5D);
#endif
}

//--------------------------------------------------------------
//1000Mbps
void miconCntl_Eth1000M(int up)
{
	debug("%s (%d)\n",__FUNCTION__,up);
#ifdef CONFIG_HTGL
	if (up){
		miconCntl_SendCmd(0x93);
	}else{
		miconCntl_SendCmd(0x92);
	}
#else
	if (up){
		miconCntl_SendCmd(0x5D);
	}else{
		miconCntl_SendCmd(0x5C);
	}
#endif
}
//--------------------------------------------------------------
//100Mbps
void miconCntl_Eth100M(int up)
{
	debug("%s (%d)\n",__FUNCTION__,up);
#ifdef CONFIG_HTGL
	if (up){
		miconCntl_SendCmd(0x91);
	}else{
		miconCntl_SendCmd(0x90);
	}
#else
	if (up){
		miconCntl_SendCmd(0x5C);
	}
#endif
}
//--------------------------------------------------------------
//10Mbps
void miconCntl_Eth10M(int up)
{
	debug("%s (%d)\n",__FUNCTION__,up);
#ifdef CONFIG_HTGL
	if (up){
		miconCntl_SendCmd(0x8F);
	}else{
		miconCntl_SendCmd(0x8E);
	}
#else
	if (up){
		miconCntl_SendCmd(0x5C);
	}
#endif
}
//--------------------------------------------------------------
//��������
void miconCntl_5f(void)
{
	debug("%s\n",__FUNCTION__);
	miconCntl_SendCmd(0x5F);
	mdelay(100);
}

//--------------------------------------------------------------
// "reboot start" signal
void miconCntl_Reboot(void)
{
	debug("%s\n",__FUNCTION__);
	miconCntl_SendCmd(0x43);
}
#if 0
//--------------------------------------------------------------
// Raid recovery start
void miconCntl_RadiRecovery(void)
{
	debug("%s\n",__FUNCTION__);
#ifdef CONFIG_HTGL
	miconCntl_SendUart(0x61); // a
	miconCntl_SendUart(0x61); // a
	miconCntl_SendUart(0x38); // 8
	miconCntl_SendUart(0x30); // 0
	miconCntl_SendUart(0x34); // 4
	miconCntl_SendUart(0x31); // 1
	miconCntl_SendCmd(0x71); // q
#endif
}
//--------------------------------------------------------------
// Raid recovery finish
void miconCntl_RadiRecoveryFin(void)
{
	debug("%s\n",__FUNCTION__);
#ifdef CONFIG_HTGL
	miconCntl_SendCmd(0x70);
#endif
}
#endif

// ---------------------------------------------------------------
// Disable watchdog timer
void miconCntl_DisWDT(void)
{
	debug("%s\n",__FUNCTION__);
	miconCntl_SendCmd(0x41); // A
	miconCntl_SendCmd(0x46); // F
	miconCntl_SendCmd(0x4A); // J
	miconCntl_SendCmd(0x3E); // >
	miconCntl_SendCmd(0x56); // V
	miconCntl_SendCmd(0x3E); // >
	miconCntl_SendCmd(0x5A); // Z
	miconCntl_SendCmd(0x56); // V
	miconCntl_SendCmd(0x4B); // K
}
// ---------------------------------------------------------------
// U-Boot calls this function
int do_reset (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	disable_interrupts();
	miconCntl_Reboot();
	while (1)
		miconCntl_SendUart(0x47);	/* Wait for reboot */

}

/* vim: set ts=4: */
