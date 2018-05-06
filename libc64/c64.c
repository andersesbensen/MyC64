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
//#include "sid_wrapper.h"
#include "mysid.h"
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

#define dbg_printf(a,...)
extern const unsigned char kernal_bin[];
extern const unsigned char basic_bin[];
extern const unsigned char chargen_bin[];

uint8_t color_ram[1024]; //0.5KB SRAM (1K*4 bit) Color RAM


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

ALL_STATIC uint8_t ram[NUM_4K_BLOCKS*4096];



#define RAM0 (uint8_t*)&ram[0x0000]
#define RAM1 (uint8_t*)&ram[0x1000]
#define RAM2 (uint8_t*)&ram[0x2000]
#define RAM3 (uint8_t*)&ram[0x3000]
#define RAM4 (uint8_t*)&ram[0x4000]
#define RAM5 (uint8_t*)&ram[0x5000]
#define RAM6 (uint8_t*)&ram[0x6000]
#define RAM7 (uint8_t*)&ram[0x7000]
#define RAM8 (uint8_t*)&ram[0x8000]
#define RAM9 (uint8_t*)&ram[0x9000]
#define RAMA (uint8_t*)&ram[0xA000]
#define RAMB (uint8_t*)&ram[0xB000]
#define RAMC (uint8_t*)&ram[0xC000]

/*#define RAMD (uint8_t*)&ram[0x0000]
#define RAME (uint8_t*)&ram[0x1000]
#define RAMF (uint8_t*)&ram[0x2000]
*/
#define RAMD (uint8_t*)&ram[0xD000]
#define RAME (uint8_t*)&ram[0xE000]
#define RAMF (uint8_t*)&ram[0xF000]




static uint8_t** map;
uint8_t* memory_layouts[8][16] = {
    { //bank 0
        RAM0,RAM1,RAM2,RAM3,
        RAM4,RAM5,RAM6,RAM7,
        RAM8,RAM9,RAMA,RAMB,
        RAMC,RAMD,RAME,RAMF,
    },

    { //bank 1
        RAM0,RAM1,RAM2,RAM3,
        RAM4,RAM5,RAM6,RAM7,
        RAM8,RAM9,RAMA,RAMB,
        RAMC,(uint8_t*)&chargen_bin[0x0000],RAME,RAMF,
    },

    { //bank 2
        RAM0,RAM1,RAM2,RAM3,
        RAM4,RAM5,RAM6,RAM7,
        RAM8,RAM9,RAMA,RAMB,
        RAMC,(uint8_t*)&chargen_bin[0x0000],(uint8_t*)&kernal_bin[0x0000],(uint8_t*)&kernal_bin[0x1000]
    },

    { //bank 3
        RAM0,RAM1,RAM2,RAM3,
        RAM4,RAM5,RAM6,RAM7,
        RAM8,RAM9,(uint8_t*)&basic_bin[0x0000], (uint8_t*)&basic_bin[0x1000],
        RAMC,(uint8_t*)&chargen_bin[0x0000],(uint8_t*)&kernal_bin[0x0000],(uint8_t*)&kernal_bin[0x1000]
    },

    { //bank 4
        RAM0,RAM1,RAM2,RAM3,
        RAM4,RAM5,RAM6,RAM7,
        RAM8,RAM9,RAMA,RAMB,
        RAMC,RAMD,RAME,RAMF,
    },

    { //bank 5
        RAM0,RAM1,RAM2,RAM3,
        RAM4,RAM5,RAM6,RAM7,
        RAM8,RAM9,RAMA,RAMB,
        RAMC,0,   RAME,RAMF,
    },

    { //bank 6
        RAM0,RAM1,RAM2,RAM3,
        RAM4,RAM5,RAM6,RAM7,
        RAM8,RAM9,RAMA,RAMB,
        RAMC,0,(uint8_t*)&kernal_bin[0x0000],(uint8_t*)&kernal_bin[0x1000]
    },

    { //bank 7
        RAM0,RAM1,RAM2,RAM3,
        RAM4,RAM5,RAM6,RAM7,
        RAM8,RAM9,(uint8_t*)&basic_bin[0x0000], (uint8_t*)&basic_bin[0x1000],
        RAMC,0,(uint8_t*)&kernal_bin[0x0000],(uint8_t*)&kernal_bin[0x1000]
    },

};

int32_t pla_read32(int address) {

  int page = (address >>12) & 0xF;
  int sub_addr=address& 0xFFF;
  //printf("read32 Address %04x\n",address);

  if(sub_addr > 0xFFC) {
    uint32_t a =*(uint32_t*)&(map[page][0xFFC]);
    uint32_t b =*(uint32_t*)&(map[page+1][0]);
    uint64_t c = (uint64_t)b << 32L | a;
    int shift = (address & 3)*8;
    return (c >> shift) & 0xffffffff;

  } else {
    return *(uint32_t*)&(map[page][sub_addr]);
  }
}

typedef uint8_t (*read_func_t)(uint16_t);
typedef void (*write_func_t)(uint16_t,uint8_t);

uint8_t color_read(uint16_t address) {
  return color_ram[address & 0x3FF];
}

uint8_t cia1_read(uint16_t address) {
  return cia_reg_read(&cia1, address & 0xF);
}

uint8_t cia2_read(uint16_t address) {
  return cia_reg_read(&cia2, address & 0xF);
}

uint8_t ram_read(uint16_t address) {
  return memory_layouts[0][0xE][address];
}

void
color_write(uint16_t address, uint8_t value)
{
  color_ram[address & 0x3FF] = value;
}

void
cia1_write(uint16_t address, uint8_t value)
{
  cia_reg_write(&cia1, address & 0xF, value);
}

void
cia2_write(uint16_t address, uint8_t value)
{
  cia_reg_write(&cia2, address & 0xF, value);
}

void
ram_write(uint16_t address, uint8_t value)
{
  memory_layouts[0][0xE][address] = value;
}


read_func_t io_read[16] =
    { vic_reg_read,vic_reg_read ,vic_reg_read,vic_reg_read,
      sid_read    ,sid_read     ,sid_read    ,sid_read,
      color_read  ,color_read,color_read,color_read,
      cia1_read, cia2_read,ram_read, ram_read
};

write_func_t io_write[16] =
    { vic_reg_write,vic_reg_write ,vic_reg_write,vic_reg_write,
      sid_write    ,sid_write     ,sid_write    ,sid_write,
      color_write  ,color_write,color_write,color_write,
      cia1_write, cia2_write,ram_write, ram_write
};


extern int tape_sense;
uint8_t
read6502(uint16_t address)
{

  if(address ==1) {
    if(tape_sense) {
      return map[0][1] | PROT_CASETTE_SENSE;
    } else {
      return map[0][1] & ~PROT_CASETTE_SENSE;
    }
  }

  int page = (address >>12) & 0xF;
  int sub_addr=address& 0xFFF;

  uint8_t* ptr=map[page];
  if(ptr) {
    //printf("read Address %04x val %02x\n",address,ptr[sub_addr]);

    return ptr[sub_addr];
  } else {
    return io_read[sub_addr>>8](address & 0x3FF);
  }
  return 0;
}

int page_touch=0;
void
write6502(uint16_t address, uint8_t value)
{
  uint8_t* ptr;
  //printf("write Address %04x val %02x\n",address,value);

  if (address == 1 && (value != ram[1]))
  {
    //printf("bank %i\n",value & 7);
    map = memory_layouts[value & 7];
   /* printf("PORT0 %x\n",value);
    printf("DRIVE %s\n", value & PROT_CASETTE_CTRL ? "off" : "on");*/
  }

  int page = (address >>12) & 0xF;
  int sub_addr=address& 0xFFC;

  ptr=map[page];

  /*
   * If ROM is visible to the CPU during a write procedure, the ROM will
   * be read but, any data is written to the the underlying RAM. This
   * principle is particularly significant to understanding how the
   * I/O registers are addressed.
   */

  if((ptr==basic_bin || ptr == kernal_bin) ||
    (ptr==&basic_bin[0x1000] || ptr == &kernal_bin[0x1000]) ||
    (ptr==chargen_bin)) {
    ptr=memory_layouts[0][page];
  }

  if( (uint32_t)ptr ) {
   /* int align = (address & 3)*8;
    uint32_t* v = &ptr[sub_addr];

    *v &= ~(0xFF << align);
    *v |= value << align;
*/
    page_touch |= 1<<page;

    ptr[address&0xFFF] = value;;
  } else {
    //printf("IO write %04x %02x\n", address,value);
    return io_write[sub_addr>>8](address & 0x3FF,value);
  }
}



int clock_tick;

void
c64_init()
{
  printf("C64 init\n");

  vic_init();
  sid_init();
  memset(ram, 0, sizeof(ram));
  ram[0] = 0x2f;
  ram[1] = 0x37;

  map=memory_layouts[7];
  cia_init();

  reset6502();
  clock_tick = 0;
  sid_clock();

}


void
c64_joy1_press(int key, int state) {

  if(key<0 || key>4) return;
  if(state) {
    joystick1 |= 1<<key;
  } else {
    joystick1 &= ~(1<<key);
  }
}

void
c64_joy2_press(int key, int state) {

  if(key<0 || key>4) return;
  if(state) {
    joystick2 |= 1<<key;
  } else {
    joystick2 &= ~(1<<key);
  }
}

void
c64_key_press(int key, int state)
{
  if(key < 64) {
    if(state) {
      key_matrix2[key /8] |= (1<<(key & 7));
    } else {
      key_matrix2[key / 8] &= ~(1<<(key & 7));

    }
  }
}



#ifndef __arm__
void c64_load_prg(const char* file) {
  FILE*f;
  uint16_t offset;
  uint8_t buffer[0x10000];

  f = fopen(file,"rb");
  fseek(f, 0L, SEEK_END);
  size_t sz = ftell(f);
  fseek(f, 0L, SEEK_SET);

  fread(&offset,2,1,f);

  page_touch=0;
  dbg_printf("Program file size is %zU\n",sz);
  fread(buffer,sz-2,1,f);

  printf("Writing from %6x to  %6x\n", offset,offset+sz-2);

  for(int i=0; i < sz-2; i++) {
    int addr = offset + i;
    uint8_t* ptr = (uint8_t*)memory_layouts[0][addr >> 12];
    ptr[addr & 0xFFF] = buffer[i];
    //write6502(offset+i,buffer[i]);
  }
  //fread(&ram[offset],sz-2,1,f);
  fclose(f);
}
#endif

void
c65_run_frame()
{
  //40ms

#define CHUNK_MS (20)
#define CLOCKS_PR_CHUNK  (985248) / (1000/CHUNK_MS)


#ifndef __arm__
  struct timeval time1;
  struct timeval time2;
  struct timeval delta;
  gettimeofday(&time1, 0);
#endif

  int cpu_halt_clocks = 0;

#if 1
  uint32_t line_buf[64];
  for(int i=0; i < 312; i++) {
    vic_line(i,line_buf);
    vic_translate_line(i,line_buf);
  }
  vic_screen_draw_done();

  #else
  for (int i = 0; i < CLOCKS_PR_CHUNK; i++)
  {
    clock_tick++;
    if (cpu_halt_clocks == 0)
    {
      cpu_halt_clocks = step6502();
    }
    cpu_halt_clocks--;

    cpu_halt_clocks += vic_clock();

    cia_clock();


    sid_clock();
  }
#endif

#ifndef __arm__
  gettimeofday(&time2, 0);

  timersub(&time2, &time1, &delta);

  if (delta.tv_usec < CHUNK_MS*1000) //100ms
  {
    //dbg_printf("Time spent %i\n",actual_time);
    usleep(CHUNK_MS*1000 - delta.tv_usec);
  }
#endif
}

