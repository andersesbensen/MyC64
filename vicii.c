/*
 * vicii.c
 *
 *  Created on: Feb 24, 2017
 *      Author: aes
 */
#include "vicii.h"
#include <stdio.h>
#include <fake6502.h>
uint8_t vic_regs[47];

#define MSBsX  0x10
#define CTRL1  0x11
#define RASTER 0x12
#define LPX    0x13
#define LPY    0x14
#define SPR_EN 0x15
#define CTRL2  0x16
#define SP_EXP 0x17
#define M_PTR  0x18
#define IRQ    0x19
#define IRQ_EN 0x1a
#define BO_COLOR  0x20
#define BG_COLOR0 0x21
#define BG_COLOR1 0x22
#define BG_COLOR2 0x23
#define BG_COLOR3 0x24

/**************** control registers **********/
#define ECM      (vic_regs[CTRL1] & 0x40)
#define BMM      (vic_regs[CTRL1] & 0x20)
#define DEN      (vic_regs[CTRL1] & 0x10)
#define RSEL     (vic_regs[CTRL1] & 0x08)
#define YSCROLL  (vic_regs[CTRL1] & 0x07)

#define RES      (vic_regs[CTRL2] & 0x20)
#define MCM      (vic_regs[CTRL2] & 0x10)
#define CSEL     (vic_regs[CTRL2] & 0x08)
#define XSCROLL  (vic_regs[CTRL2] & 0x7)

#define mode (vic_regs[CTRL1] & 0x60) |  (vic_regs[CTRL2] & 0x10)

/*
RSEL|  Display window height   | First line  | Last line
----+--------------------------+-------------+----------
  0 | 24 text lines/192 pixels |   55 ($37)  | 246 ($f6)
  1 | 25 text lines/200 pixels |   51 ($33)  | 250 ($fa)

CSEL|   Display window width   | First X coo. | Last X coo.
----+--------------------------+--------------+------------
  0 | 38 characters/304 pixels |   31 ($1f)   |  334 ($14e)
  1 | 40 characters/320 pixels |   24 ($18)   |  343 ($157)
*/
typedef struct  {
  int first_line; //First line of screen
  int last_line;

  int first_x;
  int last_x;

  int screen_x;
  int screen_y;

  int n_lines; //Total number of line signal
  int n_visible_lines; //Numner of visible linex in signal
} vic_t;


int screen_width = 320;
int screen_height = 200;
int cycles_per_line = 63;
int number_of_lines = 312;
int first_line=48;
int visible_pixels = 403;

int RST; //Raster watch
int RASTER_Y;

extern uint8_t color_ram[1024]; //0.5KB SRAM (1K*4 bit) Color RAM

/*extern uint8_t read6502(uint16_t address);
extern void write6502(uint16_t address, uint8_t value);*/
/**
 0000    0.0   0   0
 0001  255.0   0   0
 0010  79.6875   -13.01434923670814368   +31.41941843272073796
 3  159.375   +13.01434923670814368   -31.41941843272073796
 4  95.625    +24.04738177749708281   +24.04738177749708281
 5  127.5     -24.04738177749708281   -24.04738177749708281
 6  63.75     +34.0081334493  0
 7  191.25    -34.0081334493  0
 8  95.625    -24.04738177749708281   +24.04738177749708281
 9  63.75     -31.41941843272073796   +13.01434923670814368
 A  127.5     -13.01434923670814368   +31.41941843272073796
 B  79.6875    0                        0
 C  119.53125  0                        0
 D  191.25    -24.04738177749708281   -24.04738177749708281
 E  119.53125 +34.0081334493            0
 F  159.375     0                       0




 */




uint16_t vm_ptr; //Videomatrix pointer
uint16_t c_ptr; //Character generator
uint16_t b_ptr; //Bitmap generator



const uint32_t rgb_palette[16] = {
    0x000000FF,
    0xFFFFFFFF,
    0x68372BFF,
    0x70A4B2FF,
    0x6F3D86FF,
    0x588D43FF,
    0x352879FF,
    0xB8C76FFF,
    0x6F4F25FF,
    0x433900FF,
    0x9A6759FF,
    0x444444FF,
    0x6C6C6CFF,
    0x9AD284FF,
    0x6C5EB5FF,
    0x959595FF,
};

void vic_init() {
  memset(vic_regs,0,sizeof(vic_regs));
};

void vic_reg_write(uint16_t address, uint8_t value) {

  if(value != vic_regs[address]) {
      printf("VIC reg write %4.4x = %4.4x\n",address,value);
  }

  switch(address) {
  case 0x18:
    vm_ptr = (value & 0xF0) << (10-4) ; //1K block
    c_ptr = (value & 0xE) <<  (11-1);  //2K block
    b_ptr = (value & 0xE) <<  (13-1);  //8K block
    break;
  case CTRL1:
    RST &= ~0x100;
    RST |= (value & 0x80)<< 1;
    break;
  case RASTER:
    RST &= ~0xFF;
    RST |= value;
    break;
  case IRQ:
    vic_regs[address] &= ~value;
    return;
  }

  vic_regs[address] = value;
}


uint8_t vic_reg_read(uint16_t address) {
  printf("VIC reg read %4.4x line %i\n",address,RASTER_Y);
  switch(address){
  case CTRL1:
    printf (" raster %x\n",RASTER_Y);
    return (vic_regs[address] & ~0x80) | ((RASTER_Y & 0x100) >> 1);
  case RASTER:
    return RASTER_Y & 0xFF;
  case IRQ:
    return vic_regs[address] | 0x70;
  default:
    return vic_regs[address];
  }
}

uint32_t pixelbuf[403][512];

int vic_clock() {
  static int X =0;
  int Y = RASTER_Y;
  uint8_t color;
  uint8_t pixels; //8 pixels to be drawin in this cycle
  int stun_cycles = 0;

  if ( (X>=5) && (X < 45)  &&
      (Y >= first_line) && (Y < (screen_height+ first_line)) && DEN)
  {
    //Read Character
    int cx = X - 5 ;
    int cy = (Y-first_line) / 8;
    uint8_t c = vic_ram_read(vm_ptr + cy*40 + cx);
    uint8_t bit_color = color_ram[cy*40 + cx];

    //If this a "bad line"
    if(X==0 && ( (Y&7) == 0)) {
      stun_cycles = 40;
    }
    //Read Pixel
    pixels = vic_ram_read(c_ptr + c*8 + (Y & 7)  );
    for(int j =0;j < 8; j++) {
      color = ( pixels & (1<<(7-j))  ) ? bit_color : vic_regs[BG_COLOR0] & 0xF;
      pixelbuf[Y][8*X+j] =htonl(rgb_palette[color]);
    }
  } else {
    //outside screen area
    color = vic_regs[BO_COLOR] & 0xF;
    for(int j =0;j < 8; j++) {
      pixelbuf[Y][X*8+j] =htonl(rgb_palette[color]);
    }
  }

  X++;


  if(X > 63) {
    X =0;

    RASTER_Y++;
    /*Check for raster irq*/

    if(RST == RASTER_Y) {
      vic_regs[IRQ] |= 0x01;
      if(vic_regs[IRQ_EN] & 1) {
        vic_regs[IRQ] |= 0x80;
        irq6502();
      }
    }

    if(RASTER_Y == number_of_lines) {
      RASTER_Y = 0;
    }
  }

  return stun_cycles;
}
