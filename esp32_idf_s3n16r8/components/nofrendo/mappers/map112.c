/* Minibox NTDEC/ASDER adapter, LGPL-2.0; see COPYING.LIB.
 * Register behavior references are recorded in ../README.md. */
#include <string.h>
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes.h>

static uint8 selected, outer_chr, banks[8];

static void map112_sync(void)
{
   mmc_bankrom(8, 0x8000, banks[0]);
   mmc_bankrom(8, 0xA000, banks[1]);
   mmc_bankrom(8, 0xC000, mmc_getinfo()->rom_banks * 2 - 2);
   mmc_bankrom(8, 0xE000, MMC_LASTBANK);
   mmc_bankvrom(2, 0x0000, banks[2] >> 1);
   mmc_bankvrom(2, 0x0800, banks[3] >> 1);
   for (int i = 4; i < 8; ++i)
      mmc_bankvrom(1, 0x1000 + (i - 4) * 0x400,
                   banks[i] | ((outer_chr & (1 << i)) ? 0x100 : 0));
}

static void map112_write(uint32 address, uint8 value)
{
   switch (address & 0xE001)
   {
   case 0x8000:
      selected = value & 7;
      break;
   case 0xA000:
      banks[selected] = value;
      map112_sync();
      break;
   case 0xC000:
      outer_chr = value;
      map112_sync();
      break;
   case 0xE000:
      if (value & 1) ppu_mirror(0, 0, 1, 1);
      else ppu_mirror(0, 1, 0, 1);
      break;
   default:
      break;
   }
}

static void map112_init(void)
{
   selected = outer_chr = 0;
   memset(banks, 0, sizeof(banks));
   ppu_mirror(0, 1, 0, 1);
   map112_sync();
}

static map_memwrite map112_memwrite[] = {
   {0x8000, 0xFFFF, map112_write},
   {-1, -1, NULL},
};

mapintf_t map112_intf = {
   .number = 112, .name = "NTDEC/ASDER",
   .init = map112_init, .mem_write = map112_memwrite,
};
