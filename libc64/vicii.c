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



static const unsigned char BitReverseTable256[] =
{
  0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0,
  0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8, 0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8,
  0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,
  0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC,
  0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2, 0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2,
  0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
  0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6,
  0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE, 0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE,
  0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
  0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9,
  0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5, 0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5,
  0x0D, 0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
  0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3,
  0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB, 0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB,
  0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,
  0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF
};

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
const uint8_t luma_table[] =
  { 0, 24, 8, 16, 8, 16, 8, 16, 8, 8, 16, 8, 8, 16, 8, 16 };

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
int first_line = 51;
int last_line = 250;

const int visible_pixels = 403;

static int RST; //Raster watch
static int RASTER_Y;
static int CYCLE;
static int X;//X coordinate
static int VC = 0;
static int VCBASE = 0;
static int RC = 0; //Row counter
static int VMLI = 0; //Video matrix line index
static int BA; //Bad line

static uint16_t VM; //Videomatrix pointer
static uint16_t c_ptr; //Character generator
static int16_t video_ram[40];

//static int SPRITE_X[8];
//static int SPRITE_Y[8];

typedef struct
{
  uint8_t MC; //"MC" (MOB Data Counter) is a 6 bit counter that can be loaded from MCBASE.
  uint16_t MCBASE; //"MCBASE" (MOB Data Counter Base) is a 6 bit counter with reset input.
  int X;
  int Y;
  uint32_t pixels;
} sprite_state_t;

sprite_state_t sprites[8];

extern uint8_t color_ram[1024]; //0.5KB SRAM (1K*4 bit) Color RAM
extern uint8_t ram[NUM_4K_BLOCKS * 4096];
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
  { SWAP_UINT32(0x000000FFUL), //black
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
void inline EMIT_PIXEL4(int x)
{
  luma_count++;
  if(luma_count == (32/4)) luma_buffer= luma_bufferA;
  *luma_buffer++ = luma_table[x];
}
#define pixelbuf_p luma_buffer
#define SYNC(x)
#else
uint32_t pixelbuf[312][512];
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
  if (((bank & 1) == 0) && ((addr14 & 0xf000) == 0x1000))
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
  memset(vic_regs, 0, sizeof(vic_regs));
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
      sprites[address >> 1].Y = value;
    }
    else
    {
      sprites[address >> 1].X &= ~0xFF;
      sprites[address >> 1].X |= value;
    }
  }

  switch (address)
  {
  case MSBsX:
    for (int i = 0; i < 8; i++)
    {
      sprites[i].X &= ~0x100;
      if ((value >> i) & 1)
      {
        sprites[i].X |= 0x100;
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

    if(RSEL) {
      first_line=51;
      last_line=250;
    } else {
      first_line = 55;
      last_line=246;
    }
    /*printf("RSEL %i\n",RSEL);*/

    break;
  case CTRL2:
    printf("CSEL=%i XSCROLL=%i\n",CSEL,XSCROLL);

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

inline void
emit4_pixels(uint8_t pixel_data, uint8_t color0, uint8_t color1)
{
  uint32_t pixels = 0;
  uint8_t color;

  for (int j = 0; j < 4; j++)
  {
    color = (pixel_data >> 1) & 0x1 ? color1 : color0;
    pixels |= luma_table[color];
    pixels <<= 8;
  }

  EMIT_PIXEL4(pixels);
}

inline void
emit4_mcpixels(uint8_t pixel_data, uint8_t color0, uint8_t color1, uint8_t color2, uint8_t color3)
{
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
    pixels <<= 8;
  }

  EMIT_PIXEL4(pixels);
  EMIT_PIXEL4(pixels);
}

/**
 * Each bit in pixel data represent a pixel
 */
inline void
emit8_pixels(uint8_t pixel_data, uint8_t color0, uint8_t color1)
{
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

inline void
emit8_mcpixels(uint8_t pixel_data, uint8_t color0, uint8_t color1, uint8_t color2, uint8_t color3)
{
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
    return address & 1 ? sprites[address >> 1].Y & 0xFF : sprites[address >> 1].X & 0xFF;
  }

  switch (address)
  {
  case MSBsX: //MSB of sprite X
    {
      int r = 0;
      for (int i = 0; i < 8; i++)
      {
        if (sprites[i].X & 0x100)
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

static int
sprite_engine(int i,uint32_t* pixel)
{
  sprite_state_t* s = &sprites[i];
  int stun_cycles = 0;
  /*In the first phases of cycle 55 and 56, the VIC checks for every sprite
   if the corresponding MxE bit in register $d015 is set and the Y
   coordinate of the sprite (odd registers $d001-$d00f) match the lower 8
   bits of RASTER. If this is the case and the DMA for the sprite is still
   off, the DMA is switched on, MCBASE is cleared, and if the MxYE bit is
   set the expansion flip flip is reset.*/
  if ((CYCLE == 55) && (vic_regs[SPR_EN] & (1 << i)) && (s->Y == (RASTER_Y & 0xFF)))
  {
    s->MCBASE = 0;
  }

  /*
   *  In the first phase of cycle 58, the MC of every sprite is loaded from
   *  its belonging MCBASE (MCBASE->MC) and it is checked if the DMA for the
   *  sprite is turned on and the Y coordinate of the sprite matches the lower
   *  8 bits of RASTER. If this is the case, the display of the sprite is
   *  turned on.
   */
  if ((s->X>>3) == (X>>3) && (s->MCBASE < 63))
  {

    //printf("x %3i: %3i \n",i,s->X);
    uint16_t MP = vic_ram_read(VM | 0x3f8 | i)<<6; //8 bits

    s->pixels =  vic_ram_read( MP  | (s->MCBASE + 0)) << 24;
    s->pixels |= vic_ram_read( MP  | (s->MCBASE + 1)) << 16;
    s->pixels |= vic_ram_read( MP  | (s->MCBASE + 2)) << 8;

    stun_cycles+=3;
    //printf("Line read(%02i,%02i) %08x %x\n",i,s->MCBASE,s->pixels,s->X & 7);

    s->MCBASE += 3;
    s->pixels = s->pixels >> (s->X & 7);
  }

  if (s->pixels )
  {
    // printf("%i %i %i %08x\n", X,s->X/8+10,X - s->X/8, s->pixels);
    //Check multicolor
    if (vic_regs[MxMC] & (1 << i))
    {
      for (int j = 0; j < 8; j += 2)
      {
        uint8_t color;
        switch ( s->pixels & 0xc0000000)
        {
        case 0:
          pixel++;
          pixel++;
          continue;
        case 1<<30: //Sprite multicolor 0 ($d025)
          color = vic_regs[0x25];
          break;
        case 2<<30: //Sprite color ($d027-$d02e)
          color = vic_regs[0x27 + i];
          break;
        case 3<<30: //Sprite multicolor 1 ($d026)
          color = vic_regs[0x26];
          break;
        }

        *pixel++ = rgb_palette[color];
        *pixel++ = rgb_palette[color];
        if(((vic_regs[MxXE] & (1<<i)) == 0) || (j&1)  ) {
          s->pixels = s->pixels<<2;
        }
      }
    }
    else
    {
      uint8_t color = vic_regs[0x27 + i];
      for (int j = 0; j < 8; j++)
      {
        if (s->pixels & 0x80000000)
        {
          *pixel = rgb_palette[color];
        }
        pixel++;

        if(((vic_regs[MxXE] & (1<<i)) == 0) || (j&1)  ) {
          s->pixels = s->pixels<<1;
        }

      }
    }
  }

  return stun_cycles;
}

int
vic_clock()
{
  uint8_t g_data; //8 pixels to be draw in in this cycle
  int stun_cycles = 0;
  int color0 = 0;
  int color1 = 0;
  int color2 = 0;
  int color3 = 0;

  if (RASTER_Y < 8 || RASTER_Y > (7 + 43 + 200 + 49) || CYCLE < 10)
  {
    //Outside visible area

    pixelbuf_p += 8;
  }
  else if (RASTER_Y < (first_line) || RASTER_Y > (last_line) || CYCLE < 16 || CYCLE >= 56)
  { //Border
    //outside screen area
    color0 = vic_regs[BO_COLOR] & 0xF;
    emit8_pixels(0, color0, 0);

  }
  else
  { //Display state
    int multi_color=0;
    //This is c-data bits
    uint8_t D = video_ram[VMLI] & 0xFF; //8 bits
    uint8_t bit_color = (video_ram[VMLI] >> 8) & 0xF; //4bits
    if (BMM) //Standard bitmap mode (ECM/BMM/MCM=0/1/0)
    {
      //Read Pixel
      g_data = vic_ram_read((c_ptr & 0x2000) | (VC << 3) | RC);

      if (MCM && (ECM == 0))
      {
        color0 = vic_regs[BG_COLOR0];
        color1 = (D >> 4) & 0xF;
        color2 = (D >> 0) & 0xF;
        color3 = bit_color & 0xF;
        multi_color = 1;
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

        multi_color = 1;
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

    if (multi_color)
    {
      emit8_mcpixels(g_data, color0, color1, color2, color3);
    }
    else
    {
      emit8_pixels(g_data, color0, color1);
    }

    /*4. VC and VMLI are incremented after each g-access in display state.*/
    VC++;
    VMLI++;
  }

  /* Draw sprites*/
  if (vic_regs[SPR_EN])
  {
    for (int i = 0; i < 8; i++)
    {
      stun_cycles += sprite_engine(i, pixelbuf_p-8);
    }
  }

  /*
   * In the first phase of cycle 14 of each line, VC is loaded from VCBASE
   * (VCBASE->VC) and VMLI is cleared. If there is a Bad Line Condition in
   * this phase, RC is also reset to zero.
   */
  if (CYCLE == 14)
  {
    VC = VCBASE;
    VMLI = 0;

    if (BA)
    {
      RC = 0;
    }
  }

  /*If there is a Bad Line Condition in cycles 12-54, BA is set low and the
   c-accesses are started. Once started, one c-access is done in the second
   phase of every clock cycle in the range 15-54. The read data is stored
   in the video matrix/color line at the position specified by VMLI. These
   data is internally read from the position specified by VMLI as well on
   each g-access in display state.*/
  if (BA && (CYCLE == 12))
  {

    //If this a "bad line"
    stun_cycles += 40;
    for (int j = 0; j < 40; j++)
    {
      video_ram[j] = (color_ram[VCBASE + j] << 8) | vic_ram_read(VM | (VCBASE + j));
    }
  }

  /* In the first phase of cycle 58, the VIC checks if RC=7. If so, the video
   logic goes to idle state and VCBASE is loaded from VC (VC->VCBASE). If
   the video logic is in display state afterwards (this is always the case
   if there is a Bad Line Condition), RC is incremented.
   */
  if (CYCLE == 58)
  {
    if (RC == 7)
    {
      VCBASE = VC;
    }
    RC++;
  }

  CYCLE++;
  X = (X + 8) & 0x1ff;

  if (CYCLE > 63)
  {
    CYCLE = 0;
    X=0x194 + 8; //First X-coord of a line
    /*Check for raster irq*/
    RASTER_Y++;

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

    /**
     *  A Bad Line Condition is given at any arbitrary clock cycle, if at the
     * negative edge of �0 at the beginning of the cycle RASTER >= $30 and RASTER
     * <= $f7 and the lower three bits of RASTER are equal to YSCROLL and if the
     * DEN bit was set during an arbitrary cycle of raster line $30.
     */
    if ((RASTER_Y > 0x30) && (RASTER_Y < 0xf7) && ((RASTER_Y & 7) == YSCROLL))
    {
      BA = 1;
    }
    else
    {
      BA = 0;
    }

    HSYNC;

    if (RASTER_Y == number_of_lines)
    {
      vic_screen_draw_done();
      RASTER_Y = 0;
      VCBASE = 0;
      VSYNC;
    }
  }

  return stun_cycles;
}
