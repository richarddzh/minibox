/*
** Nofrendo (c) 1998-2000 Matthew Conte (matt@conte.com)
**
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of version 2 of the GNU Library General 
** Public License as published by the Free Software Foundation.
**
** This program is distributed in the hope that it will be useful, 
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
** Library General Public License for more details.  To obtain a 
** copy of the GNU Library General Public License, write to the Free 
** Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
** Any permitted reproduction of these routines, in whole or in part,
** must bear this legend.
**
**
** map1.c
**
** mapper 1 interface
** $Id: map001.c,v 1.2 2001/04/27 14:37:11 neil Exp $
*/

#include <string.h>
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>

/* TODO: WRAM enable ala Mark Knibbs:
   ==================================
The SNROM board uses 8K CHR-RAM. The CHR-RAM is paged (i.e. it can be split
into two 4Kbyte pages).

The CRA16 line of the MMC1 is connected to the /CS1 pin of the WRAM. THIS MEANS
THAT THE WRAM CAN BE ENABLED OR DISABLED ACCORDING TO THE STATE OF THE CRA16
LINE. The CRA16 line corresponds to bit 4 of MMC1 registers 1 & 2.

The WRAM is enabled when the CRA16 line is low, and disabled when CRA16 is
high. This has implications when CHR page size is 4K, if the two page numbers
set have different CRA16 states (e.g. reg 1 bit 4 = 0, reg 2 bit 4 = 1). Then
the WRAM will be enabled and disabled depending on what CHR address is being
accessed.

When CHR page size is 8K, bit 4 of MMC1 register 1 (and not register 2, because
page size is 8K) controls whether the WRAM is enabled or not. It must be low
to be enabled. When the WRAM is disabled, reading from and writing to it will
not be possible.
*/

/* TODO: roll this into something... */
static int bitcount = 0;
static uint8 latch = 0;
static uint8 regs[4];
static int bank_select;
static uint8 lastreg;

static void map1_sync(void)
{
   switch (regs[0] & 3)
   {
   case 0: ppu_mirror(0, 0, 0, 0); break;
   case 1: ppu_mirror(1, 1, 1, 1); break;
   case 2: ppu_mirror(0, 1, 0, 1); break;
   case 3: ppu_mirror(0, 0, 1, 1); break;
   }
   if (regs[0] & 0x10)
   {
      mmc_bankvrom(4, 0x0000, regs[1]);
      mmc_bankvrom(4, 0x1000, regs[2]);
   }
   else
      mmc_bankvrom(8, 0x0000, regs[1] >> 1);

   /* Common SxROM boards, including the outer PRG bit of 512 KiB SUROM. */
   bank_select = mmc_getinfo()->rom_banks > 16 ? (regs[1] & 0x10) : 0;
   int last = mmc_getinfo()->rom_banks - 1;
   if (last > bank_select + 15) last = bank_select + 15;
   switch ((regs[0] >> 2) & 3)
   {
   case 0:
   case 1:
      mmc_bankrom(32, 0x8000, (bank_select + (regs[3] & 0x0E)) >> 1);
      break;
   case 2:
      mmc_bankrom(16, 0x8000, bank_select);
      mmc_bankrom(16, 0xC000, bank_select + (regs[3] & 0x0F));
      break;
   case 3:
      mmc_bankrom(16, 0x8000, bank_select + (regs[3] & 0x0F));
      mmc_bankrom(16, 0xC000, last);
      break;
   }
}

static void map1_write(uint32 address, uint8 value)
{
   if (value & 0x80)
   {
      regs[0] |= 0x0C;
      bitcount = 0;
      latch = 0;
      map1_sync();
      return;
   }
   latch |= (value & 1) << bitcount++;
   if (bitcount == 5)
   {
      regs[(address >> 13) - 4] = latch;
      bitcount = 0;
      latch = 0;
      map1_sync();
   }
}

static void map1_init(void)
{
   bitcount = 0;
   latch = 0;

   memset(regs, 0, sizeof(regs));

   map1_write(0x8000, 0x80);
}

static void map1_getstate(SnssMapperBlock *state)
{
   state->extraData.mapper1.registers[0] = regs[0];
   state->extraData.mapper1.registers[1] = regs[1];
   state->extraData.mapper1.registers[2] = regs[2];
   state->extraData.mapper1.registers[3] = regs[3];
   state->extraData.mapper1.latch = latch;
   state->extraData.mapper1.numberOfBits = bitcount;
}


static void map1_setstate(SnssMapperBlock *state)
{
   regs[0] = state->extraData.mapper1.registers[0];
   regs[1] = state->extraData.mapper1.registers[1];
   regs[2] = state->extraData.mapper1.registers[2];
   regs[3] = state->extraData.mapper1.registers[3];
   latch = state->extraData.mapper1.latch;
   bitcount = state->extraData.mapper1.numberOfBits;
   map1_sync();
}

static map_memwrite map1_memwrite[] =
{
   { 0x8000, 0xFFFF, map1_write },
   {     -1,     -1, NULL }
};

mapintf_t map1_intf =
{
   1, /* mapper number */
   "MMC1", /* mapper name */
   map1_init, /* init routine */
   NULL, /* vblank callback */
   NULL, /* hblank callback */
   map1_getstate, /* get state (snss) */
   map1_setstate, /* set state (snss) */
   NULL, /* memory read structure */
   map1_memwrite, /* memory write structure */
   NULL /* external sound device */
};

/*
** $Log: map001.c,v $
** Revision 1.2  2001/04/27 14:37:11  neil
** wheeee
**
** Revision 1.1  2001/04/27 12:54:40  neil
** blah
**
** Revision 1.1.1.1  2001/04/27 07:03:54  neil
** initial
**
** Revision 1.1  2000/10/24 12:19:32  matt
** changed directory structure
**
** Revision 1.8  2000/10/22 19:46:50  matt
** mirroring bugfix
**
** Revision 1.7  2000/10/22 19:17:46  matt
** mapper cleanups galore
**
** Revision 1.6  2000/10/22 15:03:13  matt
** simplified mirroring
**
** Revision 1.5  2000/10/21 19:33:38  matt
** many more cleanups
**
** Revision 1.4  2000/07/15 23:52:19  matt
** rounded out a bunch more mapper interfaces
**
** Revision 1.3  2000/07/10 05:29:03  matt
** cleaned up some mirroring issues
**
** Revision 1.2  2000/07/06 02:48:43  matt
** clearly labelled structure members
**
** Revision 1.1  2000/07/04 23:11:45  matt
** initial revision
**
*/
