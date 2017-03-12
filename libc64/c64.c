/*
 * main.c
 *
 *  Created on: Feb 24, 2017
 *      Author: aes
 */

#include "fake6502.h"
#include "vicii.h"
#include <stdio.h>
#include "cia.h"
#include "sys/time.h"
#include "c64.h"
#include "tape.h"

#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

#define dbg_printf(a,...)
extern unsigned char kernal_bin[];
extern unsigned char basic_bin[];
extern unsigned char chargen_bin[];

uint8_t color_ram[1024]; //0.5KB SRAM (1K*4 bit) Color RAM

#ifdef __arm__
#define NUM_4K_BLOCKS 7
#else
#define NUM_4K_BLOCKS 16
#endif
uint8_t ram[NUM_4K_BLOCKS*4096];

/**
 * MOS 6510 PORT bits
 */
#define PORT_LORAM         0x01
#define PORT_HIRAM         0x02
#define PORT_CHREN         0x04
#define PORT_CASETTE_WRITE 0x08
#define PROT_CASETTE_SENSE 0x10
#define PROT_CASETTE_CTRL  0x20


#define CIA2_A_CLK      (1<<6)
#define CIA2_A_DATA     (1<<7)
#define CIA2_A_DATA_OUT (1<<5)
#define CIA2_A_CLK_OUT  (1<<4)
#define CIA2_A_ATN_OUT  (1<<3)


cia_t cia1;
cia_t cia2;
uint8_t key_pressed_row =0;
uint8_t key_pressed_col =0;

typedef enum
{
  RAM, BASICROM, IO, KERNALROM, CHARROM
} mem_dev_t;

mem_dev_t area_map[8][3] =
  {
    { RAM, RAM, RAM },
    { RAM, CHARROM, RAM },
    { RAM, CHARROM, KERNALROM },
    { BASICROM, CHARROM, KERNALROM },
    { RAM, RAM, RAM },
    { RAM, IO, RAM },
    { RAM, IO, KERNALROM },
    { BASICROM, IO, KERNALROM } };

#ifndef __arm__
void c64_load_prg(const char* file) {
  FILE*f;
  uint16_t offset;

  f = fopen(file,"rb");
  fseek(f, 0L, SEEK_END);
  size_t sz = ftell(f);
  fseek(f, 0L, SEEK_SET);

  fread(&offset,2,1,f);

  dbg_printf("Program file size is %zU\n",sz);
  fread(&ram[offset],sz-2,1,f);
  fclose(f);
}
#endif

mem_dev_t
addr_to_dev(uint16_t address)
{
  int bank = (ram[1] & 7);
  if (address >= 0xA000 && address < 0xC000)
  {
    return area_map[bank][0];
  }
  if ((address >= 0xD000) && (address < 0xE000))
  {
    //dbg_printf("IO area %i %i\n",bank,area_map[bank][1]);
    return area_map[bank][1];
  }
  if (address >= 0xE000)
  {
    return area_map[bank][2];
  }
  else
  {
    return RAM;
  }
}

uint8_t
read6502(uint16_t address)
{
  mem_dev_t dev = addr_to_dev(address);

  // if( (address == 0xDC00) || (address == 0xDC01) ) {

  if (address == 0x1)
  {
    //dbg_printf("PORT read %x\n", ram[1] | (1 << 4) );
    return ram[1] |PROT_CASETTE_SENSE;
  }
  switch (dev)
  {
  case RAM:
    // dbg_printf("Read RAM %4.4x %2.2x\n",address, ram[address]);

    return ram[address];
  case BASICROM:
    return basic_bin[address - 0xA000];
  case IO:
    if (address < 0xD400)
    {
      int value = vic_reg_read(address & 0x3f);
//      dbg_printf("Read VIC %4.4x %2.2x\n", address, value);

      return value;
    }
    else if (address < 0xD800)
    {
      dbg_printf("Read SID\n");
      //SID
    }
    else if (address < 0xDC00)
    {
      return color_ram[address - 0xD800];
    }
    else if (address < 0xdd00)
    {

      //CIA 1
      int x = cia_reg_read(&cia1, address & 0xF);
      //dbg_printf("Read  CIA1 %4.4x %2.2x\n", address, x);

      return x;
    }
    else if (address < 0xde00)
    {
      //CIA 2
      int x = cia_reg_read(&cia2, address & 0xF);
      //dbg_printf("Read  CIA2 %4.4x %2.2x\n", address, x);
      return x;
    }
    return 0;
  case KERNALROM:
    //dbg_printf("READ %4.4x\n",address);

    return kernal_bin[address - 0xE000];
  case CHARROM:
    return chargen_bin[address - 0xD000];
  }

  return 0;
}

void
write6502(uint16_t address, uint8_t value)
{
  mem_dev_t dev = addr_to_dev(address);


  /*if (address == 1 && (value != ram[1]))
  {
    dbg_printf("Banking changed %x\n", value);
  }*/
  switch (dev)
  {
  /*
   * If ROM is visible to the CPU during a write procedure, the ROM will
   * be read but, any data is written to the the underlying RAM. This
   * principle is particularly significant to understanding how the
   * I/O registers are addressed.
   */
  case BASICROM:
  case KERNALROM:
  case CHARROM:
  case RAM:

    if((address >> 12) > NUM_4K_BLOCKS) {
      return;
    }
    ram[address] = value;
    return;
  case IO:
    if (address < 0xD400)
    {
      //dbg_printf("Write VIC %4.4x %2.2x\n",address,value);
      vic_reg_write(address & 0x3f, value);
      return;
    }
    else if (address < 0xD800)
    {
      //SID

      dbg_printf("Write SID %4.4x %2.2x\n", address, value);
    }
    else if (address < 0xDC00)
    {
      // dbg_printf("Color RAM addr %4.4x\n",address- 0xD800);

      color_ram[address & 0x3ff] = value;
    }
    else if (address < 0xdd00)
    {
      cia_reg_write(&cia1, address & 0xF, value);

      //dbg_printf("Write CIA1 %4.4x %2.2x\n", address, value);
      //CIA 1
    }
    else if (address < 0xde00)
    {
      cia_reg_write(&cia2, address & 0xF, value);

      //dbg_printf("Write CIA2 %4.4x %2.2x\n", address, value);
      //CIA 2
    }
    return;
  }
}

uint8_t
vic_ram_read(uint16_t address)
{
  uint8_t bank;
  uint16_t addr14 = address & 0x3FFF;

  bank = (~cia2.PRA) & 3;
  /*
   * The VIC has only 14 address lines, so it can only address 16KB of memory.
   * It can access the complete 64KB main memory all the same because the 2
   * missing address bits are provided by one of the CIA I/O chips (they are the
   * inverted bits 0 and 1 of port A of CIA 2). With that you can select one of
   * 4 16KB banks for the VIC at a time.
   */
  // dbg_printf("bank %i\n",bank);
  if (((bank & 1) == 0) && (addr14 >= 0x1000) && (addr14 < 0x2000))
  {
    return chargen_bin[address & 0xFFF];
  }
  else
  {
    return ram[(bank << 14) | (addr14)];
  }
}

int clock_tick;

void
c64_init()
{
  vic_init();
  memset(ram, 0, sizeof(ram));
  ram[0] = 0x2f;
  ram[1] = 0x37;

  memset(&cia1, 0, sizeof(cia_t));
  memset(&cia2, 0, sizeof(cia_t));
  cia1.name = "CIA1";
  cia2.name = "CIA2";

  reset6502();
  clock_tick = 0;
}

uint64_t key_matrix;

uint8_t key_matrix2[8];
void
c64_key_press(int key, int state)
{
  if(key < 64) {
    if(state) {
      key_matrix2[key /8] |= (1<<(key & 7));

      key_matrix |= (1UL <<key);
    } else {
      key_matrix &= ~(1UL <<key);

      key_matrix2[key / 8] &= ~(1<<(key & 7));

    }
  }
}

extern uint32_t clockticks6502;
void
c65_run_frame()
{
#ifndef __arm__
  struct timeval time1;
  struct timeval time2;
  struct timeval delta;
  gettimeofday(&time1, 0);
#endif

  int cpu_halt_clocks = 0;

  for (int i = 0; i < 40 * 403; i++)
  {
    clock_tick++;
    if (cpu_halt_clocks == 0)
    {
      if (cia1.IRQ & 0x80)
      {
        irq6502();
      }
      cpu_halt_clocks = step6502();
    }
    cpu_halt_clocks--;

    cpu_halt_clocks += vic_clock();

    cia_clock(&cia1);
    if (cia_clock(&cia2))
    {
      nmi6502();
    }


    uint8_t key=0x0;
    for(int i=0; i < 8; i++) {
      if( (cia1.PRA & (1<<i)) == 0) {
        key |= key_matrix2[i];
      }
    }
    cia1.PRB = 0xFF & ~key;

  }

#ifndef __arm__
  gettimeofday(&time2, 0);

  timersub(&time2, &time1, &delta);

  if (delta.tv_usec < 40 * 403)
  {
    //dbg_printf("Time spent %i\n",actual_time);
    usleep((40 * 403 - delta.tv_usec));
  }
#endif
}

