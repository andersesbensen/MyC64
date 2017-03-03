/*
 * main.c
 *
 *  Created on: Feb 24, 2017
 *      Author: aes
 */

#include "fake6502.h"
#include "vicii.h"
#include "stdio.h"
#include "cia.h"
#include "sys/time.h"
#include "c64.h"
#include <unistd.h>
#include <string.h>

extern unsigned char kernal_bin[];
extern unsigned char basic_bin[];
extern unsigned char chargen_bin[];

uint8_t color_ram[1024]; //0.5KB SRAM (1K*4 bit) Color RAM

uint8_t ram[0x10000];

/**
 * MOS 6510 PORT bits
 */
#define PORT_LORAM         0x01
#define PORT_HIRAM         0x02
#define PORT_CHREN         0x04
#define PORT_CASETTE_WRITE 0x08
#define PROT_CASETTE_SENSE 0x10
#define PROT_CASETTE_CTRL  0x20

cia_t cia1;
cia_t cia2;
char key_pressed_row;
char key_pressed_col;

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
    //printf("IO area %i %i\n",bank,area_map[bank][1]);
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
    printf("PORT read %x\n", ram[1] | (1 << 4) );
    return ram[1] | (1 << 4);
  }

  if (address == 0xe544)
  {
    printf("ClearScreen\n");
  }
  if ((address & 0xFFF0) == 0xFFF0)
  {
    printf("IRQ at %x\n", address);
  }

  switch (dev)
  {
  case RAM:
    // printf("Read RAM %4.4x %2.2x\n",address, ram[address]);

    return ram[address];
  case BASICROM:
    return basic_bin[address - 0xA000];
  case IO:
    if (address < 0xD400)
    {
      int value = vic_reg_read(address & 0x3f);
      printf("Read VIC %4.4x %2.2x\n", address, value);

      return value;
    }
    else if (address < 0xD800)
    {
      printf("Read SID\n");
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
      printf("Read  CIA1 %4.4x %2.2x\n", address, x);

      return x;
    }
    else if (address < 0xde00)
    {
      //CIA 2
      int x = cia_reg_read(&cia2, address & 0xF);
      printf("Read  CIA2 %4.4x %2.2x\n", address, x);
      return x;
    }
    else if (address < 0xdf00)
    {
      //IO 1
      printf("Read IO1\n");

    }
    else if (address < 0xdd00)
    {
      //IO 2
      printf("Read IO2\n");

    }
    return 0;
  case KERNALROM:
    //printf("READ %4.4x\n",address);

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

  /*if( (address == 0xDC00) || (address == 0xDC01)) {
   printf("Write RAM %4.4x %2.2x\n",address,value);
   }*/

  if (address == 1 && (value != ram[1]))
  {
    printf("Banking changed %x\n", value);
  }
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
//    printf("Write RAM %4.4x %2.2x\n",address,value);

    ram[address] = value;
    return;
  case IO:
    if (address < 0xD400)
    {
      //printf("Write VIC %4.4x %2.2x\n",address,value);
      vic_reg_write(address & 0x3f, value);
      return;
    }
    else if (address < 0xD800)
    {
      //SID

      printf("Write SID %4.4x %2.2x\n", address, value);
    }
    else if (address < 0xDC00)
    {
      // printf("Color RAM addr %4.4x\n",address- 0xD800);

      color_ram[address & 0x3ff] = value;
    }
    else if (address < 0xdd00)
    {
      cia_reg_write(&cia1, address & 0xF, value);

      printf("Write CIA1 %4.4x %2.2x\n", address, value);
      //CIA 1
    }
    else if (address < 0xde00)
    {
      cia_reg_write(&cia2, address & 0xF, value);

      printf("Write CIA2 %4.4x %2.2x\n", address, value);
      //CIA 2
    }
    else if (address < 0xdf00)
    {
      printf("Write IO1\n");
      //IO 1
    }
    else if (address < 0xdd00)
    {
      printf("Write IO2\n");
      //IO 2
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
  // printf("bank %i\n",bank);
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

void
c64_key_press(char key, int state)
{
  static int n=0;
  if(key == 4) {
    key = 0xD; //Return
  }
  if (state)
  {
    printf("%i: Key press %i\n",n++, key);

    key_pressed_row = 1 << (key & 0xF);
    key_pressed_col = 1 << ((key & 0xF0) >> 8);

    if(key == 0) {
      nmi6502();
    } else {
      ram[0xc6]++;
      ram[0x277]=key;
    }
  }
  else
  {
    key_pressed_row = 0;
    key_pressed_col = 0;
  }

  /*cia1.regs_s.PRA =0xFF;
   cia1.regs_s.PRB =0xFF;
   cia2.regs_s.PRA =0xFF;
   cia2.regs_s.PRB =0xFF;*/

//  nmi6502();
}

extern uint32_t clockticks6502;
void
c65_run_frame()
{
  struct timeval time1;
  struct timeval time2;
  struct timeval delta;
  int cpu_halt_clocks = 0;
  gettimeofday(&time1, 0);

  for (int i = 0; i < 40 * 403; i++)
  {
    clock_tick++;
    if (cpu_halt_clocks == 0)
    {
      if (cia1.IRQ & 0x80)
      {
        //irq6502();
      }
      cpu_halt_clocks = step6502();
    }
    cpu_halt_clocks--;

    cpu_halt_clocks += vic_clock();

    if (cia_clock(&cia1))
    {
    }

    if (cia_clock(&cia2))
    {
      nmi6502();
    }


#define CIA2_A_CLK      (1<<6)
#define CIA2_A_DATA     (1<<7)
#define CIA2_A_DATA_OUT (1<<5)
#define CIA2_A_CLK_OUT  (1<<4)
#define CIA2_A_ATN_OUT  (1<<3)
//    cia2.PRA |= 0x90;


    if( cia1.PRA & 0x4) {
      cia1.PRB = ~cia1.PRA & ~2;
    } else {
      cia1.PRB = ~cia1.PRA;
    }

    cia2.PRA |= (CIA2_A_CLK | CIA2_A_DATA);



    //cia1.PRB = cia1.PRA; //cia1.PRA & key_pressed_row ? ~key_pressed_col : 0xFF;
  }

  gettimeofday(&time2, 0);

  timersub(&time2, &time1, &delta);

  if (delta.tv_usec < 40 * 403)
  {
    //printf("Time spent %i\n",actual_time);
    usleep((40 * 403 - delta.tv_usec));
  }
}

