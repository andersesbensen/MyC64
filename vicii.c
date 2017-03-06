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



#define VICII_PAL_NORMAL_FIRST_DISPLAYED_LINE        0x10   /* 16 */
#define VICII_PAL_NORMAL_LAST_DISPLAYED_LINE         0x11f  /* 287 */
#define VICII_PAL_FULL_FIRST_DISPLAYED_LINE          0x08   /* 8 */
#define VICII_PAL_FULL_LAST_DISPLAYED_LINE           0x12c  /* 300 */
#define VICII_PAL_DEBUG_FIRST_DISPLAYED_LINE         0x00   /* 0 */
#define VICII_PAL_DEBUG_LAST_DISPLAYED_LINE          0x137  /* 311 */


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
int X =0; //X coordinate

int SPRITE_X[8];
int SPRITE_Y[8];


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




uint16_t VM; //Videomatrix pointer
uint16_t c_ptr; //Character generator
uint16_t b_ptr; //Bitmap generator
#define SWAP_UINT32(val) \
 ( (((val) >> 24) & 0x000000FF) | (((val) >>  8) & 0x0000FF00) | \
 (  ((val) <<  8) & 0x00FF0000) | (((val) << 24) & 0xFF000000) )



const uint32_t rgb_palette[16] = {
    SWAP_UINT32(0x000000FF),
    SWAP_UINT32(0xFFFFFFFF),
    SWAP_UINT32(0x68372BFF),
    SWAP_UINT32(0x70A4B2FF),
    SWAP_UINT32(0x6F3D86FF),
    SWAP_UINT32(0x588D43FF),
    SWAP_UINT32(0x352879FF),
    SWAP_UINT32(0xB8C76FFF),
    SWAP_UINT32(0x6F4F25FF),
    SWAP_UINT32(0x433900FF),
    SWAP_UINT32(0x9A6759FF),
    SWAP_UINT32(0x444444FF),
    SWAP_UINT32(0x6C6C6CFF),
    SWAP_UINT32(0x9AD284FF),
    SWAP_UINT32(0x6C5EB5FF),
    SWAP_UINT32(0x959595FF),
};

void vic_init() {
  memset(vic_regs,0,sizeof(vic_regs));
  RASTER_Y = 200;
};

void vic_reg_write(uint16_t address, uint8_t value) {

  if(value != vic_regs[address]) {
      printf("VIC reg write %4.4x = %4.4x\n",address,value);
  }


  if(address<16) {
    if(address & 1) {
      SPRITE_Y[address>>1] = value;
    } else {
      SPRITE_X[address>>1] &= ~0xFF;
      SPRITE_X[address>>1] |= value;
    }
    return;
 }
  switch(address) {
  case 0x16:
    for(int i=0; i < 8; i++) {
      SPRITE_X[i] &=~0x100;
      if(value & (1<<i)) {
        SPRITE_X[i] |= 0x100;
      }
    }
    break;
  case 0x18:
    VM = (value & 0xF0) << (10-4) ; //1K block
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
  //printf("VIC reg read %4.4x line %i\n",address,RASTER_Y);
  if(address<16) {
    return address & 1 ? SPRITE_Y[address>>1] & 0xFF : SPRITE_X[address>>1] & 0xFF;
  }

  switch(address){
  case 16: //MSB of sprite X
  {
    int r =0;
    for(int i=0; i < 8; i++) {
      if(SPRITE_X[i] & 0x80) r|=(1<<i);
    }
    return r;
  }
  case CTRL1:
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
  int Y = RASTER_Y;
  uint8_t color;
  uint8_t g_data; //8 pixels to be drawin in this cycle
  int stun_cycles = 0;

  if ( (X>=5) && (X < 45)  &&
      (Y >= first_line) && (Y < (screen_height+ first_line)) && DEN)
  {
    int RC = Y&7;

    //Read Character
    int cx = X - 5 ;
    int cy = (Y-first_line) / 8;
    int VC = cy*40 + cx;

    //This is c-data bits
    uint8_t D = vic_ram_read(VM | VC); //8 bits
    uint8_t bit_color = color_ram[VC]; //4bits

    //If this a "bad line"
    if(X==0 && ( RC == 0)) {
      stun_cycles = 40;
    }
    if(BMM ) //Standard bitmap mode (ECM/BMM/MCM=0/1/0)
    {
      //Read Pixel
      g_data = vic_ram_read((c_ptr & 0x2000) + (VC<<3) |RC  );

      int color1 = (D >> 4) &0xF;
      int color0 = (D >> 0) &0xF;

      if(MCM) {
        for(int j =0;j < 4; j++) {
          switch(g_data & (3 << (2*j))) {
          case 0:
            color = vic_regs[BG_COLOR0]; break;
          case 1:
            color = color1; break;
          case 2:
            color = color0; break;
          case 3:
            color = bit_color & 0xF; break;
          }
          pixelbuf[Y][8*X+2*j] = rgb_palette[color];
          pixelbuf[Y][8*X+2*j+1] =rgb_palette[color];
        }
      } else {
        for(int j =0;j < 8; j++) {
          color = ( g_data & (1<<(7-j))  ) ? color1 : color0;
          pixelbuf[Y][8*X+j] =rgb_palette[color];
        }
      }
    } else { //Text mode

      //Read Pixel
      g_data = vic_ram_read(c_ptr + (D<<3) + RC  );
      if (MCM && (bit_color & 0x80))
      { //3.7.3.2. Multicolor text mode (ECM/BMM/MCM=0/0/1)
        for (int j = 0; j < 4; j++)
        {
          switch (g_data & (3 << 2 * (3-j)))
          {
          case 0:
            color = vic_regs[BG_COLOR0];
            break;
          case 1:
            color = vic_regs[BG_COLOR1];
            break;
          case 2:
            color = vic_regs[BG_COLOR2];
            break;
          case 3:
            color = bit_color & 3;
            break;
          }

          pixelbuf[Y][8 * X + 2 * j] = rgb_palette[color];
          pixelbuf[Y][8 * X + 2 * j + 1] = rgb_palette[color];
        }
      } else {
        for(int j =0;j < 8; j++) {
          color = ( g_data & (1<<(7-j))  ) ? bit_color : vic_regs[BG_COLOR0] & 0xF;
          pixelbuf[Y][8*X+j] =rgb_palette[color];
        }
      }
    }
    if(ECM) {
      //printf("ECM text\n");
    }


    /*Draw sprites */
    for(int i = 0; i < 8; i++) {
      if(vic_regs[SPR_EN] & (1<<i)) {
      /*
       * The 63 bytes of sprite data necessary for displaying 24�21 pixels are
       * stored in memory in a linear fashion: 3 adjacent bytes form one line of the
       * sprite.
       */


        int spry = RASTER_Y - SPRITE_Y[i];
        if( spry>=0 && spry < 21) {
          int sprx = cx*8-SPRITE_X[i];
          if(sprx >= 0 && sprx < 24) {
            uint8_t MP = vic_ram_read(VM | 0x3f8 | i); //8 bits
            int MC = sprx / 8 + spry*3;
            uint8_t pixel = vic_ram_read( (MP << 6) | MC);
            uint8_t color =vic_regs[0x27 + i];

            for(int j =0;j < 8; j++) {
              if(pixel & (1<< (7-j))) {
                pixelbuf[Y][X*8+j] = rgb_palette[color];
              }
            }
          }
        }
      }
    }

  } else {
    //outside screen area
    color = vic_regs[BO_COLOR] & 0xF;
    for(int j =0;j < 8; j++) {
      pixelbuf[Y][X*8+j] =rgb_palette[color];
    }
  }



  X++;


  if(X > 63) {
    X =0;


    if(RST == RASTER_Y) {
      //printf("VIC Raster watch %i %i\n",RST,vic_regs[IRQ_EN]);
      vic_regs[IRQ] |= 0x01;
      if(vic_regs[IRQ_EN] & 1) {
        vic_regs[IRQ] |= 0x80;
        printf("VIC IRQ\n");
        irq6502();
      }
    }

    RASTER_Y++;
    /*Check for raster irq*/

    if(RASTER_Y == number_of_lines) {
      vic_screen_draw_done();
      RASTER_Y = 0;
    }
  }

  return stun_cycles;
}
