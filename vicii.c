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
 * Cycl-# 6                   1 1 1 1 1 1 1 1 1 1 2 2 2 2 2 2 2 2 2 2 3 3 3 3 3 3 3 3 3 3 4 4 4 4 4 4 4 4 4 4 5 5 5 5 5 5 5 5 5 5 6 6 6 6
 3 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 1
 _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _
 �0 _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _
 __
 IRQ   ________________________________________________________________________________________________________________________________
 ________________________                                                                                      ____________________
 BA                         ______________________________________________________________________________________
 _ _ _ _ _ _ _ _ _ _ _ _ _ _ _                                                                                 _ _ _ _ _ _ _ _ _ _
 AEC _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _________________________________________________________________________________ _ _ _ _ _ _ _ _ _

 VIC i 3 i 4 i 5 i 6 i 7 i r r r r rcgcgcgcgcgcgcgcgcgcgcgcgcgcgcgcgcgcgcgcgcgcgcgcgcgcgcgcgcgcgcgcgcgcgcgcgcgcgcgcg i i 0 i 1 i 2 i 3
 6510  x x x x x x x x x x x x X X X                                                                                 x x x x x x x x x x

 Graph.                      |===========01020304050607080910111213141516171819202122232425262728293031323334353637383940=========

 X coo. \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\
       1111111111111111111111111110000000000000000000000000000000000000000000000000000000000000000111111111111111111111111111111111111111
 89999aaaabbbbccccddddeeeeff0000111122223333444455556666777788889999aaaabbbbccccddddeeeeffff000011112222333344445555666677778888999
 c048c048c048c048c048c048c04048c048c048c048c048c048c048c048c048c048c048c048c048c048c048c048c048c048c048c048c048c048c048c048c048c048
 *
 */
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
typedef struct
{
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
int first_line = 48;
int visible_pixels = 403;

int RST; //Raster watch
int RASTER_Y;
int X = 0; //X coordinate


int SPRITE_X[8];
int SPRITE_Y[8];
int video_ram[40];

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

const uint32_t rgb_palette[16] =
  { SWAP_UINT32(0x000000FF), SWAP_UINT32(0xFFFFFFFF), SWAP_UINT32(0x68372BFF), SWAP_UINT32(0x70A4B2FF), SWAP_UINT32(
      0x6F3D86FF), SWAP_UINT32(0x588D43FF), SWAP_UINT32(0x352879FF), SWAP_UINT32(0xB8C76FFF), SWAP_UINT32(0x6F4F25FF),
      SWAP_UINT32(0x433900FF), SWAP_UINT32(0x9A6759FF), SWAP_UINT32(0x444444FF), SWAP_UINT32(0x6C6C6CFF), SWAP_UINT32(
          0x9AD284FF), SWAP_UINT32(0x6C5EB5FF), SWAP_UINT32(0x959595FF), };

void
vic_init()
{
  memset(vic_regs, 0, sizeof(vic_regs));
  RASTER_Y = 200;
}
;

void
vic_reg_write(uint16_t address, uint8_t value)
{

  if (value != vic_regs[address])
  {
    printf("VIC reg write %4.4x = %4.4x\n", address, value);
  }

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
    VM = ((value>>4) & 0xF) << 10; //1K block
    c_ptr = ((value>>1) & 0x7) << 11; //2K block
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

uint32_t pixelbuf[403][512];

int
vic_clock()
{
  int Y = RASTER_Y;
  uint8_t color;
  uint8_t g_data; //8 pixels to be draw in in this cycle
  int stun_cycles = 0;
  int color0;
  int color1;
  int color2;
  int color3;

  if( vic_regs[IRQ] & 0x80) {
    irq6502();
  }

  int yy = Y - first_line;
  int RC = (yy) & 7;


  if ((X >= 16) && (X < 56) && (yy >= 0) &&  (yy < screen_height ) && DEN)
  {

    //Read Character
    int cx = X - 16;
    int cy = (yy) >>3;
    if(cy >= 25) cy= 0;
    int VC = cy * 40 + cx;

    //If this a "bad line"
    if (cx == 0 && (RC == 0))
    {
      //stun_cycles += 42;
      for(int j = 0; j < 40; j++) {
        video_ram[j] = (color_ram[VC + j] << 8 ) | vic_ram_read(VM | VC + j);
      }
    }


    //This is c-data bits
    uint8_t D =  video_ram[cx] & 0xFF; //8 bits
    uint8_t bit_color = (video_ram[cx]>>8) & 0xF ; //4bits
    if (BMM) //Standard bitmap mode (ECM/BMM/MCM=0/1/0)
    {
      //Read Pixel
      g_data = vic_ram_read((c_ptr & 0x2000) | (VC << 3) | RC);


      if (MCM && (ECM==0))
      {
        color0 = vic_regs[BG_COLOR0];
        color1 = (D >> 4) & 0xF;
        color2 = (D >> 0) & 0xF;
        color3 = bit_color & 0xF;
      }
      else if(ECM==0)
      {
        color1 = (D >> 4) & 0xF;
        color0 = (D >> 0) & 0xF;
      } else {
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
        if(ECM) {
          color0 =  (D>>6) & 3 ;
        } else {
          color0 =  vic_regs[BG_COLOR0];
        }
      }
    }

    if(MCM) {
        for (int j = 0; j < 8; j += 2)
       {
         switch ((g_data >> (6-j)) & 3)
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

         pixelbuf[Y][8 * X + j] = rgb_palette[color];
         pixelbuf[Y][8 * X + j + 1] =rgb_palette[color];
       }
    } else {
      for (int j = 0; j < 8; j++)
      {
        color = (g_data << j) & 0x80  ? color1 : color0;
        pixelbuf[Y][8 * X + j] = rgb_palette[color];
      }
    }



    /*Draw sprites */
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
            if(sprx ==0 && spry==0) {
              stun_cycles+=1; //Penalty for reading MP
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
                  pixelbuf[Y][X * 8 + 6 - j] = rgb_palette[color];
                  pixelbuf[Y][X * 8 + 6 - j + 1] = rgb_palette[color];
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
                  pixelbuf[Y][X * 8 + j] = rgb_palette[color];
                }
              }
            }

          }
        }
      }
    }

  }
  else if ((X >= 11) && (X < 61))
  {
    //outside screen area
    color = vic_regs[BO_COLOR] & 0xF;
    for (int j = 0; j < 8; j++)
    {
      pixelbuf[Y][X * 8 + j] = rgb_palette[color];
    }
  }
  else
  {
    for (int j = 0; j < 8; j++)
    {
      pixelbuf[Y][X * 8 + j] = 0;
    }
  }

  X++;

  if (X > 63)
  {
    X = 0;

    /*Check for raster irq*/

    if (RST == RASTER_Y)
    {
      //printf("VIC Raster watch %i %i\n",RST,vic_regs[IRQ_EN]);
      vic_regs[IRQ] |= 0x01;
      if (vic_regs[IRQ_EN] & 1)
      {
        vic_regs[IRQ] |= 0x80;
      }
    }

    RASTER_Y++;

    if (RASTER_Y == number_of_lines)
    {
      vic_screen_draw_done();
      RASTER_Y = 0;
    }
  }

  return stun_cycles;
}
