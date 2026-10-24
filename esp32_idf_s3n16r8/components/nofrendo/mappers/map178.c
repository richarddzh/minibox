/* Minibox Waixing adapter, LGPL-2.0; see COPYING.LIB.
 * Register behavior references are recorded in ../README.md. */
#include <string.h>
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes.h>

static uint8 regs[4];

static void map178_sync(void)
{
   int outer = regs[2] << 3;
   int bank = outer | (regs[1] & 7);
   if (regs[0] & 2)
   {
      mmc_bankrom(16, 0x8000, bank);
      mmc_bankrom(16, 0xC000,
         outer | ((regs[0] & 4) ? (6 | (regs[1] & 1)) : 7));
   }
   else if (regs[0] & 4)
   {
      mmc_bankrom(16, 0x8000, bank);
      mmc_bankrom(16, 0xC000, bank);
   }
   else
      mmc_bankrom(32, 0x8000, bank >> 1);

   nes6502_context cpu;
   nes6502_getcontext(&cpu);
   cpu.mem_page[6] = mmc_getinfo()->sram + (regs[3] & 3) * 8192;
   cpu.mem_page[7] = cpu.mem_page[6] + 0x1000;
   nes6502_setcontext(&cpu);
   if (regs[0] & 1) ppu_mirror(0, 0, 1, 1);
   else ppu_mirror(0, 1, 0, 1);
}

static void map178_write(uint32 address, uint8 value)
{
   regs[address & 3] = value;
   map178_sync();
}

static void map178_init(void)
{
   memset(regs, 0, sizeof(regs));
   mmc_bankvrom(8, 0, 0);
   map178_sync();
}

static map_memwrite map178_memwrite[] = {
   {0x4800, 0x4FFF, map178_write},
   {-1, -1, NULL},
};

mapintf_t map178_intf = {
   .number = 178, .name = "Waixing",
   .init = map178_init, .mem_write = map178_memwrite,
};
