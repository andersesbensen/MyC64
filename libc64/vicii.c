/*
 * vicii.c
 *
 *  Created on: Feb 24, 2017
 *      Author: aes
 */
#include "vicii.h"
#include <stdio.h>
#include "fake6502.h"
#include "cia.h"
#include "c64.h"

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
#define MxDP   0x1b   //Sprite data priority
#define MxMC   0x1c   //Multicolor sprite
#define MxXE   0x1d   //Sprite X expansion
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
/**
 *
 *
 *
Number — name Y Pb (rel.) Pr (rel.) Number — name Y Pb (rel.) Pr (rel.)
0 — black     0 0 0                     8 — orange       0.375 −0.707  0.707
1 — white     1 0 0                     9 — brown        0.25  −0.924  0.383
2 — red     0.313 −0.383  0.924        10 — light red    0.5 −0.383  0.924
3 — cyan    0.625 0.383 −0.924         11 — dark grey    0.313 0 0
4 — purple  0.375 0.707 0.707          12 — grey         0.469 0 0
5 — green   0.5 −0.707  −0.707         13 — light green  0.75  −0.707  −0.707
6 — blue    0.25  1 0                  14 — light blue   0.469 1 0
7 — yellow  0.75  −1  0                15 — light grey   0.625 0 0
 *
 */
/*
 * luma values
0:0
1:0.313
2:0.375
3:0.5
4:0.625
5:0.25
6:0.75
7:1
*/
/*const uint8_t luma_table[] = {
    0<<3,7<<3,1<<3,4<<3,2<<3,3<<3,5<<3,6<<3,
    2<<3,5<<3,3<<3,1<<3,3<<3,6<<3,3<<3,4<<3
};*/

/*
 * array([ 0.   ,  3.   ,  0.939,  1.875,  1.125,  1.5  ,  0.75 ,  2.25 ,
        1.125,  0.75 ,  1.5  ,  0.939,  1.407,  2.25 ,  1.407,  1.875])
 */
const uint8_t luma_table[] = {
    0,  24,   8,  16,   8,  16,   8,  16,   8,   8,  16,
             8,   8,  16,   8,  16
};

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

const int screen_width = 320;
const int screen_height = 200;
const int cycles_per_line = 63;
const int number_of_lines = 312;
const int first_line = 48;
const int visible_pixels = 403;

static int RST; //Raster watch
static int RASTER_Y;
static int X ; //X coordinate
static int VCC = 0;
static int VCBASE = 0;
static uint16_t VM; //Videomatrix pointer
static uint16_t c_ptr; //Character generator
static int16_t video_ram[40];

static int SPRITE_X[8];
static int SPRITE_Y[8];

extern uint8_t color_ram[1024]; //0.5KB SRAM (1K*4 bit) Color RAM
extern uint8_t ram[NUM_4K_BLOCKS*4096];
extern const unsigned char chargen_bin[];

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


#define SWAP_UINT32(val) \
 ( (((val) >> 24) & 0x000000FF) | (((val) >>  8) & 0x0000FF00) | \
 (  ((val) <<  8) & 0x00FF0000) | (((val) << 24) & 0xFF000000) )

const uint32_t rgb_palette[16] =
  { SWAP_UINT32(0x000000FFUL),   //black
      SWAP_UINT32(0xFFFFFFFFUL), //white
      SWAP_UINT32(0x68372BFFUL), //red
      SWAP_UINT32(0x70A4B2FFUL), //cyan
      SWAP_UINT32(0x6F3D86FFUL), //purple
      SWAP_UINT32(0x588D43FFUL), //green
      SWAP_UINT32(0x352879FFUL), //blue
      SWAP_UINT32(0xB8C76FFFUL), //yellow
      SWAP_UINT32(0x6F4F25FFUL), //orange
      SWAP_UINT32(0x433900FFUL), //brown
      SWAP_UINT32(0x9A6759FFUL), //light red
      SWAP_UINT32(0x444444FFUL), //drak gray
      SWAP_UINT32(0x6C6C6CFFUL), //grey
      SWAP_UINT32(0x9AD284FFUL), //light green
      SWAP_UINT32(0x6C5EB5FFUL), //light blue
      SWAP_UINT32(0x959595FFUL), //light gray
  };

#ifdef __arm__
extern void arm_sync(int level);
extern void arm_emit_pixel(int luma,int color);
extern uint32_t* luma_buffer;
extern uint32_t* luma_bufferA;
int luma_count=0;
#define VSYNC
#define HSYNC
void inline EMIT_PIXEL4(int x) {
     luma_count++;
    if(luma_count == (32/4))   luma_buffer= luma_bufferA;
    *luma_buffer++ = luma_table[x];
  }
#define pixelbuf_p luma_buffer
#define SYNC(x)
#else
uint32_t pixelbuf[403][512];
uint32_t* pixelbuf_p;
#define VSYNC
#define HSYNC pixelbuf_p=pixelbuf[RASTER_Y]
#define EMIT_PIXEL(x) *pixelbuf_p++ = rgb_palette[x]
#define SYNC(x)
#endif

/**
 * Read a 14 bit address
 */
static uint8_t
vic_ram_read(uint16_t address)
{
  uint8_t bank;
  uint16_t addr14 = address; // & 0x3FFF;

  bank = (~cia2.PRA) & 3;
  /*
   * The VIC has only 14 address lines, so it can only address 16KB of memory.
   * It can access the complete 64KB main memory all the same because the 2
   * missing address bits are provided by one of the CIA I/O chips (they are the
   * inverted bits 0 and 1 of port A of CIA 2). With that you can select one of
   * 4 16KB banks for the VIC at a time.
   */
  // dbg_printf("bank %i\n",bank);
  //if (((bank & 1) == 0) && (addr14 >= 0x1000) && (addr14 < 0x2000))
  if (((bank & 1) == 0) && ((addr14 &0xf000) == 0x1000) )
  {
    return chargen_bin[address & 0xFFF];
  }
  else
  {
    int final_addr = (bank << 14) | (addr14);

    return ram[final_addr & ADDR_MASK];
  }
}

void
vic_init()
{
  /*for (int i = 0; i < sizeof(vic_regs); i++)
  {
    vic_regs[i] = 0;
  }*/
  memset(vic_regs,0, sizeof(vic_regs));
  RASTER_Y = 0;
  VSYNC;
  HSYNC;
}
;

void
vic_reg_write(uint16_t address, uint8_t value)
{

  /*if (value != vic_regs[address])
   {
   printf("VIC reg write %4.4x = %4.4x\n", address, value);
   }*/

  if (address < 16)
  {
    if (address & 1)
    {
      SPRITE_Y[address >> 1] = value;
    }
    else
    {
      SPRITE_X[address >> 1] &= ~0xFF;
      SPRITE_X[address >> 1] |= value;
    }
    return;
  }
  switch (address)
  {
  case 0x16:
    for (int i = 0; i < 8; i++)
    {
      SPRITE_X[i] &= ~0x100;
      if (value & (1 << i))
      {
        SPRITE_X[i] |= 0x100;
      }
    }
    break;
  case 0x18:
    VM = ((value >> 4) & 0xF) << 10; //1K block
    c_ptr = ((value >> 1) & 0x7) << 11; //2K block
    break;
  case CTRL1:
    RST &= ~0x100;
    RST |= (value & 0x80) << 1;
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


inline void emit4_pixels(uint8_t pixel_data, uint8_t color0, uint8_t color1) {
  uint32_t pixels=0;
  uint8_t color;

  for (int j = 0; j < 4; j++)
  {
    color = (pixel_data >> 1) & 0x1 ? color1 : color0;
    pixels |= luma_table[color];
    pixels <<= 8;
  }

  EMIT_PIXEL4(pixels);
}


inline void emit4_mcpixels(uint8_t pixel_data, uint8_t color0, uint8_t color1,uint8_t color2, uint8_t color3) {
  uint32_t pixels;
  uint8_t color;
  for (int j = 0; j < 4; j += 2)
  {
    switch ((pixel_data >> j) & 3)
    {
    case 0:
      color = color0;
      break;
    case 1:
      color = color1;
      break;
    case 2:
      color = color2;
      break;
    case 3:
      color = color3;
      break;
    }

    pixels |= luma_table[color];
    pixels <<=8;
  }

  EMIT_PIXEL4(pixels);
  EMIT_PIXEL4(pixels);
}



/**
 * Each bit in pixel data represent a pixel
 */
inline void emit8_pixels(uint8_t pixel_data, uint8_t color0, uint8_t color1) {
#ifdef __arm__
  emit4_pixels(pixel_data>>4,color0,color1);
  emit4_pixels(pixel_data,color0,color1);
#else
  for (int j = 0; j < 8; j++)
  {
    uint8_t color = (pixel_data << j) & 0x80 ? color1 : color0;
    EMIT_PIXEL(color);
  }
#endif
}

inline void emit8_mcpixels(uint8_t pixel_data, uint8_t color0, uint8_t color1,uint8_t color2, uint8_t color3) {
  uint8_t color;

#ifdef __arm__
  emit4_mcpixels(pixel_data>>4,color0,color1,color2,color3);
  emit4_mcpixels(pixel_data,color0,color1,color2,color3);
#else

  for (int j = 0; j < 8; j += 2)
  {
    switch ((pixel_data >> (6 - j)) & 3)
    {
    case 0:
      color = color0;
      break;
    case 1:
      color = color1;
      break;
    case 2:
      color = color2;
      break;
    case 3:
      color = color3;
      break;
    }

    EMIT_PIXEL(color);
    EMIT_PIXEL(color);
  }
#endif
}


uint8_t
vic_reg_read(uint16_t address)
{
  //printf("VIC reg read %4.4x line %i\n",address,RASTER_Y);
  if (address < 16)
  {
    return address & 1 ? SPRITE_Y[address >> 1] & 0xFF : SPRITE_X[address >> 1] & 0xFF;
  }

  switch (address)
  {
  case 16: //MSB of sprite X
    {
      int r = 0;
      for (int i = 0; i < 8; i++)
      {
        if (SPRITE_X[i] & 0x80)
          r |= (1 << i);
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

int
vic_clock()
{
  uint8_t color;
  uint8_t g_data; //8 pixels to be draw in in this cycle
  int stun_cycles = 0;
  int color0 = 0;
  int color1 = 0;
  int color2 = 0;
  int color3 = 0;

  //SYNC value
  if(X==6 && (RASTER_Y==2)) {
    SYNC(0);
    //4.7us at the beginning of each line
    //64us at the end of each frame

  }

  //Colorburst
  //if(X==7) {
    //COLOR_SEQ(0b1100);
    //LUMINA_SEQ(0)
  //}

  //if (vic_regs[IRQ] & 0x80)
  //{
  //  irq6502();
  //}

  int yy = RASTER_Y - first_line;
  unsigned int RC = (yy + YSCROLL) & 7;

  if ((X == 11) && (RC == 0))
  {

    //If this a "bad line"
    stun_cycles += 42;
    for (int j = 0; j < 40; j++)
    {
      video_ram[j] = (color_ram[VCBASE + j] << 8) | vic_ram_read(VM | (VCBASE + j));
    }
  }

  if ((X >= 16) && (X < 56) && (yy >= 0) && (yy < screen_height) && DEN)
  {

    //Read Character
    int cx = X - 16;

    //This is c-data bits
    uint8_t D = video_ram[cx] & 0xFF; //8 bits
    uint8_t bit_color = (video_ram[cx] >> 8) & 0xF; //4bits
    if (BMM) //Standard bitmap mode (ECM/BMM/MCM=0/1/0)
    {
      //Read Pixel
      g_data = vic_ram_read((c_ptr & 0x2000) | (VCC << 3) | RC);

      if (MCM && (ECM == 0))
      {
        color0 = vic_regs[BG_COLOR0];
        color1 = (D >> 4) & 0xF;
        color2 = (D >> 0) & 0xF;
        color3 = bit_color & 0xF;
      }
      else if (ECM == 0)
      {
        color1 = (D >> 4) & 0xF;
        color0 = (D >> 0) & 0xF;
      }
      else
      {
        color0 = 0;
        color1 = 0;
      }
    }
    else
    { //Text mode
      //Read Pixels
      g_data = vic_ram_read(c_ptr | (D << 3) | RC);
      if (MCM && (bit_color & 0x8))
      { //3.7.3.2. Multicolor text mode (ECM/BMM/MCM=0/0/1)
        color0 = vic_regs[BG_COLOR0];
        color1 = vic_regs[BG_COLOR1];
        color2 = vic_regs[BG_COLOR2];
        color3 = bit_color & 7;
      }
      else
      {
        color1 = bit_color;
        if (ECM)
        {
          color0 = (D >> 6) & 3;
        }
        else
        {
          color0 = vic_regs[BG_COLOR0];
        }
      }
    }

    if (MCM)
    {
      emit8_mcpixels(g_data,color0,color1,color2,color3);    }
    else
    {
      emit8_pixels(g_data,color0,color1);
    }

#ifndef __arm__
    /*Draw sprites */
    if(vic_regs[SPR_EN])
    for (int i = 0; i < 8; i++)
    {
      if (vic_regs[SPR_EN] & (1 << i))
      {
        /*
         * The 63 bytes of sprite data necessary for displaying 24�21 pixels are
         * stored in memory in a linear fashion: 3 adjacent bytes form one line of the
         * sprite.
         */

        int spry = RASTER_Y - SPRITE_Y[i];
        if (spry >= 0 && spry < 21)
        {
          int sprx = cx * 8 - SPRITE_X[i];
          if (sprx >= 0 && sprx < 24)
          {
            if (sprx == 0 && spry == 0)
            {
              stun_cycles += 1; //Penalty for reading MP
            }

            uint8_t MP = vic_ram_read(VM | 0x3f8 | i); //8 bits
            int MC = sprx / 8 + spry * 3;
            uint8_t pixel = vic_ram_read((MP << 6) | MC);

            //Check expansion
            if (vic_regs[MxMC] & (1 << i))
            {
              for (int j = 0; j < 8; j += 2)
              {
                uint8_t color;
                switch ((pixel >> j) & 3)
                {
                case 1: //Sprite multicolor 0 ($d025)
                  color = vic_regs[0x25];
                  break;
                case 2: //Sprite color ($d027-$d02e)
                  color = vic_regs[0x27 + i];
                  break;
                case 3: //Sprite multicolor 1 ($d026)
                  color = vic_regs[0x26];
                  break;
                }

                if ((pixel >> j) & 3)
                {
                  pixelbuf[RASTER_Y][X * 8 + 6 - j] = rgb_palette[color];
                  pixelbuf[RASTER_Y][X * 8 + 6 - j + 1] = rgb_palette[color];
                }
              }
            }
            else
            {
              uint8_t color = vic_regs[0x27 + i];

              for (int j = 0; j < 8; j++)
              {
                if (pixel & (1 << (7 - j)))
                {
                  pixelbuf[RASTER_Y][X * 8 + j] = rgb_palette[color];
                }
              }
            }

          }
        }
      }
    }
#endif

    //Increment video matrix counter
    VCC++;

  }
  else if ((X >= 11) && (X < 61))
  {
    //outside screen area
    color = vic_regs[BO_COLOR] & 0xF;
    emit8_pixels(0,color,0);
  }
  else
  {
    pixelbuf_p+=2;
    //emit8_pixels(0,1,0);
  }

  X++;
  if (X > 63)
  {
    X = 0;
    if (RC == 7)
    {
      VCBASE = VCC;
    }
    else
    {
      VCC = VCBASE;
    }

    /*Check for raster irq*/

    if (RST == RASTER_Y)
    {
      //printf("VIC Raster watch %i %i\n",RST,vic_regs[IRQ_EN]);
      vic_regs[IRQ] |= 0x01;
      if (vic_regs[IRQ_EN] & 1)
      {
        vic_regs[IRQ] |= 0x80;
        irq6502();
      }
    }

    RASTER_Y++;
    HSYNC;

    if (RASTER_Y == number_of_lines)
    {
      vic_screen_draw_done();
      RASTER_Y = 0;
      VCC = 0;
      VCBASE = 0;
      VSYNC;
    }
  }

  return stun_cycles;
}
