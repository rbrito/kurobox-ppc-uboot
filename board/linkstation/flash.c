/*
 * flash.c
 *
 * Flash device interface for LinkStation
 * Supports CFI flash devices using the AMD standard command set
 *
 * Copyright (C) 2006 Mihai Georgin <u-boot@linuxnotincluded.org.uk>
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
 * Based on the MTD code from the Linux kernel
 * 
 * Based on include/melco/flashd.c (linux-2.4.17_mvl21-sandpoint)
 * Copyright (C) 2001-2004  BUFFALO INC.
 */
#include <common.h>
#include <asm/io.h>
#include <mpc824x.h>

#if 0
#define DEBUG_CFI
#endif

#undef  debug
#ifdef  DEBUG_CFI
#define debug(fmt,args...)      printf(fmt,##args)
#else
#define debug(fmt,args...)
#endif  /* DEBUG_CFI */

#if CFG_MAX_FLASH_BANKS > 1
#error Only 1 flash bank supported
#endif

#define perror(fmt,args...)		printf("%s: ",__FUNCTION__);printf(fmt,##args)

#define MAX_ERASE_REGIONS		4

#define P_ID_NONE			    0
#define P_ID_INTEL_EXT		    1
#define P_ID_AMD_STD		    2
#define P_ID_INTEL_STD		    3
#define P_ID_AMD_EXT		    4
#define P_ID_MITSUBISHI_STD	  256
#define P_ID_MITSUBISHI_EXT	  257
#define P_ID_RESERVED 		65535

#define CFI_DEVICETYPE_X8	 (8 / 8)
#define CFI_DEVICETYPE_X16	(16 / 8)

#define FLASH_DATA_MASK		0xFF

#define FUJ_MANUFACT_LS		(FUJ_MANUFACT    & FLASH_DATA_MASK)
#define STM_MANUFACT_LS		(STM_MANUFACT    & FLASH_DATA_MASK)
#define MX_MANUFACT_LS		(MX_MANUFACT     & FLASH_DATA_MASK)

/* Unknown manufacturer */
#define FLASH_MAN_UNKNOWN	0xFFFF0000

/* Fujitsu MBM29PL320MT which is using the same */
/* codes as the AMD Am29LV320MT "mirror" flash */
#define AMD_ID_MIRROR_LS	(AMD_ID_MIRROR   & FLASH_DATA_MASK)
#define AMD_ID_LV320T_2_LS	(AMD_ID_LV320T_2 & FLASH_DATA_MASK)
#define AMD_ID_LV320T_3_LS	(AMD_ID_LV320T_3 & FLASH_DATA_MASK)

/* ST Micro M29W320DT and M29W320DB */
#define STM_ID_29W320DT_LS	(STM_ID_29W320DT & FLASH_DATA_MASK)
#define STM_ID_29W320DB_LS	(STM_ID_29W320DB & FLASH_DATA_MASK)

/* ST Micro M29DW324DT and M29DW324DB */
#define STM_ID_29W324DT_LS	(STM_ID_29W324DT & FLASH_DATA_MASK)
#define STM_ID_29W324DB_LS	(STM_ID_29W324DB & FLASH_DATA_MASK)

/* Macronix MX29LV320T */
#define MX_ID_LV320T_LS		(MX_ID_LV320T    & FLASH_DATA_MASK)

/* Basic Query Structure */
struct cfi_ident {
  __u8  qry[3];
  __u16 P_ID;
  __u16 P_ADR;
  __u16 A_ID;
  __u16 A_ADR;
  __u8  VccMin;
  __u8  VccMax;
  __u8  VppMin;
  __u8  VppMax;
  __u8  WordWriteTimeoutTyp;
  __u8  BufWriteTimeoutTyp;
  __u8  BlockEraseTimeoutTyp;
  __u8  ChipEraseTimeoutTyp;
  __u8  WordWriteTimeoutMax;
  __u8  BufWriteTimeoutMax;
  __u8  BlockEraseTimeoutMax;
  __u8  ChipEraseTimeoutMax;
  __u8  DevSize;
  __u16 InterfaceDesc;
  __u16 MaxBufWriteSize;
  __u8  NumEraseRegions;
  __u32 EraseRegionInfo[MAX_ERASE_REGIONS];
} __attribute__((packed));

struct cfi_private {
	__u32 base;
	int device_type;
	int addr_unlock1;
	int addr_unlock2;
	struct cfi_ident *cfiq;
	int mfr;
	int id[3]; /* Supports AMD MirrorBit flash */
	char *flash_name;
	int  wrd_wr_time;
	int  buf_wr_time;
	int  erase_time;
	int (*blk_erase)(flash_info_t *info, int s_first, int s_last);
	int (*blk_write)(flash_info_t *info, __u8 *buf, __u32 addr, int sz);
};

static inline __u8 cfi_read8(__u32 addr)
{
	return (*(volatile __u8 *)(addr));
}

static inline void cfi_write8(__u8 val, __u32 addr)
{
	*(volatile __u8 *)(addr) = val;
	sync();
}

/*
 * Sends a CFI command to a bank of flash for the given type.
 * Returns the offset to the sent command
 */
static inline __u32 cfi_cmd(__u8 cmd, __u32 cmd_addr, __u32 base, int type)
{
	__u32 addr;

	addr = base + cmd_addr * type;
	if (cmd_addr * type == 0x554)
		++addr;

	cfi_write8(cmd, addr);

	return addr - base;
}

static inline __u8 cfi_read_query(__u32 addr)
{
	return cfi_read8(addr);
}

flash_info_t flash_info[CFG_MAX_FLASH_BANKS];
static struct cfi_private cfis;
static struct cfi_ident   cfi_idents;
static struct cfi_private *cfi;

static int cfi_probe_chip(struct cfi_private *cfi);
static unsigned long cfi_amdstd_setup(struct cfi_private *cfi, int primary);
static void print_cfi_ident(struct cfi_ident *);
static int flash_amdstd_erase(flash_info_t *info, int s_first, int s_last);
static int flash_amdstd_wbuff(flash_info_t *info, __u8 *buf, __u32 addr,int sz);
static int flash_amdstd_wubyp(flash_info_t *info, __u8 *buf, __u32 addr,int sz);
static int flash_amdstd_write(flash_info_t *info, __u8 *buf, __u32 addr,int sz);



unsigned long flash_init(void)
{
	unsigned long flash_size;
	__u16 type;

	debug("%s\n", __FUNCTION__);

	cfi = &cfis;
	memset(cfi, 0, sizeof(struct cfi_private));

	cfi->base = CFG_FLASH_BASE;

	/* Identify CFI chip */
	/* Probe for X8 device first */
	cfi->device_type = CFI_DEVICETYPE_X8;
	if (cfi_probe_chip(cfi)) {
		/* The probe didn't like it */
		/* so probe for X16/X8 device */
		cfi->device_type = CFI_DEVICETYPE_X16;
		if (cfi_probe_chip(cfi)) {
			/* The probe didn't like it */
			return 0UL;
		}
	}		

	/* Check if it is AMD standard cmd set */
	type = cfi->cfiq->P_ID;
	if (type == P_ID_AMD_STD)
		flash_size = cfi_amdstd_setup(cfi, 1);
	else {
		perror("Primary cmd set is not AMD std. Trying alternate.\n");
		flash_size = 0;
	}
	if (!flash_size) {
		type = cfi->cfiq->A_ID;
		if (type == P_ID_AMD_STD)
			flash_size = cfi_amdstd_setup(cfi, 0);
		else {
			perror("Alternate cmd set is not AMD std.\n");
			return 0UL;
		}
	}

	if (flash_size && flash_size == 4*1024*1024) {
		/* Flash protection ON by default */
		flash_protect(FLAG_PROTECT_SET, cfi->base, cfi->base+flash_size-1, flash_info);

		return flash_size;
	}

	if (flash_size) {
		perror("Unsupported flash size: %d\n", flash_size);
	} else {
		perror("Vendor Command Set not supported\n");
		printf("Primary: 0x%04X, Alternate: 0x%04X\n",
				cfi->cfiq->P_ID, cfi->cfiq->A_ID);
	}
	return 0UL;
}

void flash_print_info(flash_info_t *info)
{
	int i;

	debug("%s\n", __FUNCTION__);

	printf("Flash chip: %s\n\n",
		   cfi->flash_name?cfi->flash_name:"UNKNOWN");
	print_cfi_ident(cfi->cfiq);
	printf("\nActual values used by U-Boot:\n");
	printf("Word   write timeout: %6d ms\n", cfi->wrd_wr_time);
	printf("Buffer write timeout: %6d ms\n", cfi->buf_wr_time);
	printf("Sector erase timeout: %6d ms\n", cfi->erase_time);
	printf("\nSize: %ld MiB in %d Sectors\n",info->size>>20,info->sector_count);
	printf ("  Sector Start Addresses:");
	for (i=0; i<info->sector_count; i++) {
		if (!(i % 5))
			printf ("\n   ");
		printf (" %08lX%s", info->start[i], info->protect[i]?" (RO)" : " (RW)");
	}
	printf ("\n");
}

int flash_erase(flash_info_t *info, int s_first, int s_last)
{
	return (*(cfi->blk_erase))(info, s_first, s_last);
}

int write_buff(flash_info_t *info, uchar *src, ulong addr, ulong cnt)
{
	return (*(cfi->blk_write))(info, src, addr, cnt);
}

static int cfi_probe_chip(struct cfi_private *cfi)
{
	int ofs_factor = cfi->device_type;
	__u32 base = cfi->base;
	int num_erase_regions, scount;
	int i;

	debug("%s\n", __FUNCTION__);

	cfi_cmd(0xF0, 0x00, base, cfi->device_type);
	cfi_cmd(0x98, 0x55, base, cfi->device_type);

	if (cfi_read8(base + ofs_factor * 0x10) != 'Q' ||
	    cfi_read8(base + ofs_factor * 0x11) != 'R' ||
	    cfi_read8(base + ofs_factor * 0x12) != 'Y') {
		debug("Not a CFI flash\n");
		/* Put the chip back into read array mode */
		cfi_cmd(0xF0, 0x00, base, cfi->device_type);
		return -1;
	}

	num_erase_regions = cfi_read_query(base + 0x2C * ofs_factor);
	if (!num_erase_regions) {
		perror("No erase regions\n");
		/* Put the chip back into read read array mode */
		cfi_cmd(0xF0, 0x00, base, cfi->device_type);
		return -1;
	}
	if (num_erase_regions > MAX_ERASE_REGIONS) {
		perror("Number of erase regions (%d) > MAX_ERASE_REGIONS (%d)\n",
				num_erase_regions, MAX_ERASE_REGIONS);
		/* Put the chip back into read read array mode */
		cfi_cmd(0xF0, 0x00, base, cfi->device_type);
		return -1;
	}

	cfi->cfiq = &cfi_idents;
	memset(cfi->cfiq, 0, sizeof(struct cfi_ident));	
	debug("cfi->cfiq: 0x%08X\n", cfi->cfiq);
	
	/* Read the CFI info structure */
	for (i=0; i < sizeof(struct cfi_ident) + num_erase_regions * 4; i++)
		((__u8 *)cfi->cfiq)[i] = cfi_read_query(base + (0x10 + i) * ofs_factor);
	
	/* Do any necessary byteswapping */
	cfi->cfiq->P_ID            = __le16_to_cpu(cfi->cfiq->P_ID);
	cfi->cfiq->P_ADR           = __le16_to_cpu(cfi->cfiq->P_ADR);
	cfi->cfiq->A_ID            = __le16_to_cpu(cfi->cfiq->A_ID);
	cfi->cfiq->A_ADR           = __le16_to_cpu(cfi->cfiq->A_ADR);
	cfi->cfiq->InterfaceDesc   = __le16_to_cpu(cfi->cfiq->InterfaceDesc);
	cfi->cfiq->MaxBufWriteSize = __le16_to_cpu(cfi->cfiq->MaxBufWriteSize);

#if 0
	/* Dump the information therein */
	print_cfi_ident(cfi->cfiq);
#endif

	scount = 0;
	for (i=0; i<cfi->cfiq->NumEraseRegions; i++) {
		cfi->cfiq->EraseRegionInfo[i] = __le32_to_cpu(cfi->cfiq->EraseRegionInfo[i]);
		scount += (cfi->cfiq->EraseRegionInfo[i] & 0xFFFF) + 1;
		debug("  Erase Region #%d: sector size 0x%4.4X bytes, %d sectors\n",
		       i, (cfi->cfiq->EraseRegionInfo[i] >> 8) & ~0xFF, 
		       (cfi->cfiq->EraseRegionInfo[i] & 0xFFFF) + 1);
	}
	/* Put it back into Read Mode */
	cfi_cmd(0xF0, 0, base, cfi->device_type);

	if (scount > CFG_MAX_FLASH_SECT) {
		perror("Number of sectors (%d) > CFG_MAX_FLASH_SECT (%d)\n",
				scount, CFG_MAX_FLASH_SECT);
		return -1;
	}

	debug("Found x%d device in 8-bit mode\n", cfi->device_type*8);
	
	return 0;
}

static char *vendorname(__u16 vendor) 
{
	switch (vendor) {
	case P_ID_NONE:
		return "None";
	case P_ID_INTEL_EXT:
		return "Intel/Sharp Extended";
	case P_ID_AMD_STD:
		return "AMD/Fujitsu Standard";
	case P_ID_INTEL_STD:
		return "Intel/Sharp Standard";
	case P_ID_AMD_EXT:
		return "AMD/Fujitsu Extended";
	case P_ID_MITSUBISHI_STD:
		return "Mitsubishi Standard";
	case P_ID_MITSUBISHI_EXT:
		return "Mitsubishi Extended";
	case P_ID_RESERVED:
		return "Not Allowed / Reserved for Future Use";
	default:
		return "Unknown";
	}
}

static void print_cfi_ident(struct cfi_ident *cfip)
{
	printf("CFI Query Results:\n");
	printf("Primary Vendor Command Set: 0x%4.4X (%s)\n",
			cfip->P_ID, vendorname(cfip->P_ID));
	if (cfip->P_ADR)
		printf("Primary Algorithm Table at 0x%4.4X\n", cfip->P_ADR);
	else
		printf("No Primary Algorithm Table\n");
	
	printf("Alternate Vendor Command Set: 0x%4.4X (%s)\n",
		cfip->A_ID, vendorname(cfip->A_ID));
	if (cfip->A_ADR)
		printf("Alternate Algorithm Table at 0x%4.4X\n", cfip->A_ADR);
	else
		printf("No Alternate Algorithm Table\n");
		
	printf("Vcc Min.: %d.%d V\n", cfip->VccMin >> 4, cfip->VccMin & 0xF);
	printf("Vcc Max.: %d.%d V\n", cfip->VccMax >> 4, cfip->VccMax & 0xF);
	if (cfip->VppMin) {
		printf("Vpp Min.: %d.%d V\n", cfip->VppMin >> 4, cfip->VppMin & 0xF);
		printf("Vpp Max.: %d.%d V\n", cfip->VppMax >> 4, cfip->VppMax & 0xF);
	}
	else
		printf("No Vpp line\n");
	
	printf("Typical byte/word write timeout: %d us\n",
			1<<cfip->WordWriteTimeoutTyp);
	printf("Maximum byte/word write timeout: %d us\n",
			(1<<cfip->WordWriteTimeoutMax) * (1<<cfip->WordWriteTimeoutTyp));
	
	if (cfip->BufWriteTimeoutTyp || cfip->BufWriteTimeoutMax) {
		printf("Typical full buffer write timeout: %d us\n",
				1<<cfip->BufWriteTimeoutTyp);
		printf("Maximum full buffer write timeout: %d us\n",
				(1<<cfip->BufWriteTimeoutMax) * (1<<cfip->BufWriteTimeoutTyp));
	}
	else
		printf("Full buffer write not supported\n");
	
	printf("Typical block erase timeout: %d ms\n",
			1<<cfip->BlockEraseTimeoutTyp);
	printf("Maximum block erase timeout: %d ms\n",
			(1<<cfip->BlockEraseTimeoutMax) * (1<<cfip->BlockEraseTimeoutTyp));
	if (cfip->ChipEraseTimeoutTyp || cfip->ChipEraseTimeoutMax) {
		printf("Typical chip erase timeout: %d ms\n",
				1<<cfip->ChipEraseTimeoutTyp); 
		printf("Maximum chip erase timeout: %d ms\n",
				(1<<cfip->ChipEraseTimeoutMax) * (1<<cfip->ChipEraseTimeoutTyp));
	}
	else
		printf("Chip erase not supported\n");
	
	printf("Device size: 0x%X bytes (%d MiB)\n",
			1 << cfip->DevSize, 1 << (cfip->DevSize - 20));
	printf("Flash Device Interface description: 0x%4.4X\n",cfip->InterfaceDesc);
	switch(cfip->InterfaceDesc) {
	case 0:
		printf("  - x8-only asynchronous interface\n");
		break;
	case 1:
		printf("  - x16-only asynchronous interface\n");
		break;
	case 2:
		printf("  - x8 / x16 via BYTE# with asynchronous interface\n");
		break;
	case 3:
		printf("  - x32-only asynchronous interface\n");
		break;
	case 65535:
		printf("  - Not Allowed / Reserved\n");
		break;
	default:
		printf("  - Unknown\n");
		break;
	}
	printf("Max. bytes in buffer write: %d\n", 1 << cfip->MaxBufWriteSize);
	printf("Number of Erase Block Regions: %d\n", cfip->NumEraseRegions);
}

static unsigned long cfi_amdstd_setup(struct cfi_private *cfi, int primary)
{
	flash_info_t *info = &flash_info[0];
	__u32 base         = cfi->base;
	int ofs_factor     = cfi->device_type;
	__u32 addr_et      = primary?cfi->cfiq->P_ADR:cfi->cfiq->A_ADR;
	__u8 major, minor, bootloc;
	__u32 offset, ernum, ersize;
	int i, j;

	/* Put the chip into read array mode */
	cfi_cmd(0xF0, 0x00, base, cfi->device_type);
	/* Autoselect */
	cfi_cmd(0xAA, 0x555, base, cfi->device_type);
	cfi_cmd(0x55, 0x2AA, base, cfi->device_type);
	cfi_cmd(0x90, 0x555, base, cfi->device_type);
	/* Read manufacturer and device id */
	cfi->mfr = cfi_read_query(base + 0x00 * ofs_factor);
	if ((cfi->id[0] = cfi_read_query(base + 0x01 * ofs_factor)) == 0x7E) {   
		 cfi->id[1] = cfi_read_query(base + 0x0E * ofs_factor);
		 cfi->id[2] = cfi_read_query(base + 0x0F * ofs_factor);
	}
	/* Put the chip into read array mode */
	cfi_cmd(0xF0, 0x00, base, cfi->device_type);

	/* Put the chip into read query mode */
	cfi_cmd(0x98, 0x55, base, cfi->device_type);
	/* Find the boot block location and swap the erase regions as necessary */
	major = cfi_read_query(base + (addr_et + 3) * ofs_factor);
	minor = cfi_read_query(base + (addr_et + 4) * ofs_factor);
	debug(" Amd/Fujitsu Extended Query Table v%c.%c at 0x%4.4X\n",
	       major, minor, addr_et);

	if (((major << 8) | minor) < 0x3131) {
		/* CFI version 1.0 => don't trust bootloc */
		if (cfi->id[0] & 0x80) {
			printf("Device ID is 0x%02X. Assuming broken CFI table.\n",
					cfi->id[0]);
			bootloc = 3;	/* top boot */
		} else
			bootloc = 2;	/* bottom boot */
	} else
		bootloc = cfi_read_query(base + (addr_et + 0xF) * ofs_factor);

	if (bootloc == 3 && cfi->cfiq->NumEraseRegions > 1) {
		debug("Top boot block. Swapping erase regions.\n");
		for (i=0; i<cfi->cfiq->NumEraseRegions / 2; i++) {
			int j = (cfi->cfiq->NumEraseRegions-1)-i;
			__u32 swap;
			
			swap = cfi->cfiq->EraseRegionInfo[i];
			cfi->cfiq->EraseRegionInfo[i] = cfi->cfiq->EraseRegionInfo[j];
			cfi->cfiq->EraseRegionInfo[j] = swap;
		}
	}

	/* Put the chip into read array mode */
	cfi_cmd(0xF0, 0x00, base, cfi->device_type);

	switch (cfi->device_type) {
	case CFI_DEVICETYPE_X8:
		/* X8 chip */
		cfi->addr_unlock1 = 0x555; 
		cfi->addr_unlock2 = 0x2AA; 
		break;
	case CFI_DEVICETYPE_X16:
		/* X16 chip in X8 mode */
		cfi->addr_unlock1 = 0xAAA;
		cfi->addr_unlock2 = 0x555;
		break;
	default:
		perror("Unsupported device type %d\n", cfi->device_type);
		return 0UL;
	}

	cfi->wrd_wr_time  = 1 << cfi->cfiq->WordWriteTimeoutTyp;
	cfi->wrd_wr_time *= 1 << cfi->cfiq->WordWriteTimeoutMax;
	/* Word write time is in us, convert to ms */
	cfi->wrd_wr_time  = cfi->wrd_wr_time / 1000 + 1;
	if (cfi->wrd_wr_time == 1)
		/* Account for the timer resolution which is 1 ms */
		cfi->wrd_wr_time = 2;
	cfi->buf_wr_time  = 1 << cfi->cfiq->BufWriteTimeoutTyp;
	cfi->buf_wr_time *= 1 << cfi->cfiq->BufWriteTimeoutMax;
	/* Buffer write time is in us, convert to ms */
	cfi->buf_wr_time  = cfi->buf_wr_time / 1000 + 1;
	if (cfi->buf_wr_time == 1)
		/* Account for the timer resolution which is 1 ms */
		cfi->buf_wr_time = 2;
	cfi->erase_time   = 1 << cfi->cfiq->BlockEraseTimeoutTyp;
	cfi->erase_time  *= 1 << cfi->cfiq->BlockEraseTimeoutMax;
	
	info->size = (1 << cfi->cfiq->DevSize);
	
	info->sector_count = 0;
	offset = CFG_FLASH_BASE;
	for (i=0; i < cfi->cfiq->NumEraseRegions; i++) {
		ersize = ((cfi->cfiq->EraseRegionInfo[i] >> 8) & ~0xFF);
		ernum = (cfi->cfiq->EraseRegionInfo[i] & 0xFFFF) + 1;
		
		for (j=0; j < ernum; j++) {
			info->start[info->sector_count + j] = offset;
			offset += ersize;
		}

		info->sector_count += ernum;
	}

	switch (cfi->mfr) {
	case FUJ_MANUFACT_LS:
		info->flash_id = FLASH_MAN_FUJ;
		switch (cfi->id[0]) {
		case AMD_ID_MIRROR_LS:
			if (cfi->id[1] == AMD_ID_LV320T_2_LS &&
			    cfi->id[2] == AMD_ID_LV320T_3_LS) {
				info->flash_id += FLASH_AMLV320T;
				cfi->blk_write = flash_amdstd_wbuff;
				cfi->flash_name = "FUJITSU MBM29PL32TM";
			} else
				info->flash_id += FLASH_UNKNOWN;
			break;
		default:
			info->flash_id += FLASH_UNKNOWN;
			break;
		}
		break;
	case STM_MANUFACT_LS:
		info->flash_id = FLASH_MAN_STM;
		switch (cfi->id[0]) {
		case STM_ID_29W320DT_LS:
			info->flash_id += FLASH_STMW320DT;
			cfi->blk_write = flash_amdstd_wubyp;
			cfi->flash_name = "STMICRO M29W320DT";
			break;
		case STM_ID_29W320DB_LS:
			info->flash_id += FLASH_STMW320DB;
			cfi->blk_write = flash_amdstd_wubyp;
			cfi->flash_name = "STMICRO M29W320DB";
			break;
		case STM_ID_29W324DT_LS:
			info->flash_id += FLASH_STMW324DT;
			cfi->blk_write = flash_amdstd_wubyp;
			cfi->flash_name = "STMICRO M29W324DT";
			break;
		case STM_ID_29W324DB_LS:
			info->flash_id += FLASH_STMW324DB;
			cfi->blk_write = flash_amdstd_wubyp;
			cfi->flash_name = "STMICRO M29W324DB";
			break;
		default:
			info->flash_id += FLASH_UNKNOWN;
			break;
		}
		break;
	case MX_MANUFACT_LS:
		info->flash_id = FLASH_MAN_MX;
		switch (cfi->id[0]) {
		case MX_ID_LV320T_LS:
			info->flash_id += FLASH_MXLV320T;
			cfi->blk_write = flash_amdstd_write;
			cfi->flash_name = "MXIC MX29LV320T";
			break;
		default:
			info->flash_id += FLASH_UNKNOWN;
			break;
		}
		break;
	default:
		info->flash_id = FLASH_AMD_COMP;
		break;
	}

	if ((info->flash_id & FLASH_TYPEMASK) == FLASH_UNKNOWN) {
		/* Unknown but supported CFI flash */
		cfi->flash_name = NULL;
		if (cfi->cfiq->MaxBufWriteSize)
			cfi->blk_write = flash_amdstd_wbuff;
		else
			cfi->blk_write = flash_amdstd_write;
	}

	cfi->blk_erase = flash_amdstd_erase;

	return info->size;
}

#define BIT(x) (1<<x)
/*
 * Check the flash command state
 */
static int flash_amdstd_state(__u32 addr, __u32 target, int timeout)
{
	__u32 start_time = get_timer(0);
	__u32 data;

	debug("%s\n", __FUNCTION__);

	do {
		data = cfi_read8(addr);
		if((data & BIT(7)) == (target & BIT(7)))
			return 0;
		if(data & BIT(5)) {
			data = cfi_read8(addr);
			if((data & BIT(7)) == (target & BIT(7))) 
				return 0;
			else
				return -1;
		}
	} while (get_timer(start_time) < timeout);
	return -1;
}

/*
 * Verify data written to flash
 */
static int flash_amdstd_vrfy(flash_info_t *info, __u8 *buf, __u32 addr, int sz)
{
	__u32 base = cfi->base;
	__u8  *faddr;
	long  i;

	debug("%s\n", __FUNCTION__);

	faddr = (__u8 *)addr;
	for(i=0; i < sz; i++) {
		if(faddr[i] != buf[i]) {
			printf("Flash Write verify fail at %08x. ", &faddr[i]);
			printf("Expecting: %02X, Actual: %02X\n", faddr[i], buf[i]);
			printf("Retrying...");
			cfi_cmd(0xAA, cfi->addr_unlock1, base, CFI_DEVICETYPE_X8);
			cfi_cmd(0x55, cfi->addr_unlock2, base, CFI_DEVICETYPE_X8);
			cfi_cmd(0xA0, cfi->addr_unlock1, base, CFI_DEVICETYPE_X8);
			cfi_write8(buf[i], (__u32)&faddr[i]);
			if (flash_amdstd_state((__u32)&faddr[i], buf[i], cfi->wrd_wr_time)) {
				printf("failed again\n");
				cfi_cmd(0xF0, 0, base, CFI_DEVICETYPE_X8);
				return 1;
			} else
				printf("suceeded\n");
		}
	}
	return 0;
}

/*
 * Erase flash sectors
 */
static int flash_amdstd_erase(flash_info_t *info, int s_first, int s_last)
{
	int prot, sect, nsect, flag;
	__u32 l_sect;
	__u32 base = cfi->base;

	debug("%s\n", __FUNCTION__);

	if (!info->size) {
		printf ("Flash erase: Can't erase unsupported flash\n");
		return 1;
	}

	if (s_first < 0 || s_first > s_last    ||
	    s_first > (info->sector_count - 1) ||
		s_last  > (info->sector_count - 1)) {
		printf ("Flash erase: no sectors to erase\n");
		return 1;
	}

	printf("\nFlash erase: first = %d @ 0x%08lx\n",
	        s_first, info->start[s_first]);
	printf("             last  = %d @ 0x%08lx\n", s_last, info->start[s_last]);

	nsect = s_last - s_first + 1;
	for (prot = 0, sect=s_first; sect<=s_last; ++sect)
		if (info->protect[sect])
			prot++;
	if (prot) {
		if (prot == nsect) {
			printf("Warning: All requested sectors are protected!\n");
			printf("         No sectors to erase\n");
			return 1;
		}
		else
			printf("Warning: %d protected sectors will not be erased!\n", prot);
	}
	cfi_cmd(0xF0, 0x00, base, CFI_DEVICETYPE_X8);
	udelay(1000);

	/* Disable interrupts which might cause a timeout here */
	flag = disable_interrupts();

	cfi_cmd(0xAA, cfi->addr_unlock1, base, CFI_DEVICETYPE_X8);
	cfi_cmd(0x55, cfi->addr_unlock2, base, CFI_DEVICETYPE_X8);
	cfi_cmd(0x80, cfi->addr_unlock1, base, CFI_DEVICETYPE_X8);
	cfi_cmd(0xAA, cfi->addr_unlock1, base, CFI_DEVICETYPE_X8);
	cfi_cmd(0x55, cfi->addr_unlock2, base, CFI_DEVICETYPE_X8);
	for (sect = s_first; sect <= s_last; sect++)
		if (!info->protect[sect]) {
			l_sect = info->start[sect];
			cfi_write8(0x30, l_sect);
		}
	/* Erase begins 50us after the last sector address */
	udelay(50);

	/* All erase commands sent, enable interrupts */
	if (flag)
		enable_interrupts();

	if (flash_amdstd_state(l_sect, 0xff, cfi->erase_time * nsect)) {
		printf("Flash erase: Timeout\n");
		cfi_cmd(0xF0, 0x00, base, CFI_DEVICETYPE_X8);
		return 1;
	}
	printf("Flash erase: Done\n");
	return 0;
}

/*
 * Write to flash using Write Buffer programming
 */
static int flash_amdstd_wbuff(flash_info_t *info, __u8 *buf, __u32 addr, int sz)
{
	__u32 base = cfi->base;
	__u32 wbufsz;
	__u32 size, wsize, waddr, saddr;
	__u8 *wbuf;
	int i;

	debug("%s\n", __FUNCTION__);

	size = sz;
	wbuf = buf;
	wbufsz = 1 << cfi->cfiq->MaxBufWriteSize;

	waddr = (addr + wbufsz - 1) & ~(wbufsz - 1);
	if (waddr > addr)
		wsize = waddr-addr;
	else
		wsize = wbufsz;
	if (wsize > size)
		wsize = size;
	waddr = addr;

	while (size > 0) {
		for (i = 0; i < info->sector_count; i++)
			if (waddr < info->start[i])
				break;
		saddr = info->start[i-1];

       	cfi_cmd(0xAA, cfi->addr_unlock1, base, CFI_DEVICETYPE_X8);
       	cfi_cmd(0x55, cfi->addr_unlock2, base, CFI_DEVICETYPE_X8);
		cfi_write8(0x25, saddr);
		cfi_write8(wsize-1, saddr);
		for (i = 0; i < wsize; i++)
			cfi_write8(*wbuf++, waddr++);
		cfi_write8(0x29, saddr);

    	if (flash_amdstd_state(waddr-1, *(wbuf-1), cfi->buf_wr_time)) {
			printf("Flash write buffer: Timeout\n");
			cfi_cmd(0xF0, 0x00, base, CFI_DEVICETYPE_X8);
			return 1;
		}

		size -= wsize;
		if ((wsize = wbufsz) > size)
			wsize = size;
	}
			
	return flash_amdstd_vrfy(info, buf, addr, sz);
}

/*
 * Write to flash using Unlock Bypass command sequence
 */
static int flash_amdstd_wubyp(flash_info_t *info, __u8 *buf, __u32 addr, int sz)
{
	__u32 base = cfi->base;
	__u32 waddr;
	long  i;

	debug("%s\n", __FUNCTION__);

	waddr = addr;

	cfi_cmd(0xAA, cfi->addr_unlock1, base, CFI_DEVICETYPE_X8);
	cfi_cmd(0x55, cfi->addr_unlock2, base, CFI_DEVICETYPE_X8);
	cfi_cmd(0x20, cfi->addr_unlock1, base, CFI_DEVICETYPE_X8);

	for(i=0; i < sz; i++) {
		cfi_write8(0xA0, waddr);
		cfi_write8(buf[i], waddr);
		if (flash_amdstd_state(waddr, buf[i], cfi->wrd_wr_time)) {
			printf("Flash unlock bypass write: Timeout\n");
			cfi_cmd(0x90, 0, base, CFI_DEVICETYPE_X8);
			cfi_cmd(0x00, 0, base, CFI_DEVICETYPE_X8);
			cfi_cmd(0xF0, 0, base, CFI_DEVICETYPE_X8);
			return 1;
		}
		waddr++;
	}
	cfi_cmd(0x90, 0, base, CFI_DEVICETYPE_X8);
	cfi_cmd(0x00, 0, base, CFI_DEVICETYPE_X8);
	cfi_cmd(0xF0, 0, base, CFI_DEVICETYPE_X8);

	return flash_amdstd_vrfy(info, buf, addr, sz);
}

/*
 * Write to flash using Word/Byte Program command sequence
 */
static int flash_amdstd_write(flash_info_t *info, __u8 *buf, __u32 addr, int sz)
{
	__u32 base = cfi->base;
	__u32 waddr;
	long  i;

	debug("%s\n", __FUNCTION__);

	waddr = addr;

	cfi_cmd(0xF0, 0, base, CFI_DEVICETYPE_X8);
	for (i = 0; i < sz; i++) {
		cfi_cmd(0xAA, cfi->addr_unlock1, base, CFI_DEVICETYPE_X8);
		cfi_cmd(0x55, cfi->addr_unlock2, base, CFI_DEVICETYPE_X8);
		cfi_cmd(0xA0, cfi->addr_unlock1, base, CFI_DEVICETYPE_X8);
		cfi_write8(buf[i], waddr);
		if (flash_amdstd_state(waddr, buf[i], cfi->wrd_wr_time)) {
			printf("Flash write: Timeout\n");
			cfi_cmd(0xF0, 0, base, CFI_DEVICETYPE_X8);
			return 1;
		}
		waddr++;
	}
	cfi_cmd(0xF0, 0, base, CFI_DEVICETYPE_X8);

	return flash_amdstd_vrfy(info, buf, addr, sz);
}

/* vim: set ts=4: */
