/*
 * vicii.c
 *
 *  Created on: Feb 24, 2017
 *      Author: aes
 */


#include "vicii.h"
#include <stdio.h>
#include <string.h>
#include "fake6502.h"
#include "cia.h"
#include "c64.h"

extern void sid_clock();

uint32_t pixelbuf[312][512];

static uint8_t vic_regs[47];

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
int first_x = 24;
int last_x = 343;

const int visible_pixels = 403;

static int RST; //Raster watch
static int RASTER_Y;
static int CYCLE;
static int X;//X coordinate

static uint16_t VM; //Videomatrix pointer
static uint8_t *VM_p; //Videomatrix pointer

static uint16_t c_ptr; //Character generator
static uint8_t *c_ptr_p; //Character generator
static uint8_t *c_ptr_bm_p; //Character generator

static int16_t video_ram[40];
static int mode;

typedef struct
{
  uint8_t MC; //"MC" (MOB Data Counter) is a 6 bit counter that can be loaded from MCBASE.
  uint16_t MCBASE; //"MCBASE" (MOB Data Counter Base) is a 6 bit counter with reset input.
  int X;
  int Y;
  uint32_t pixels;
} sprite_state_t;

static sprite_state_t sprites[8];

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


/**
 * Read a 14 bit address
 */

static uint8_t*
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
    return (uint8_t*)&chargen_bin[address & 0xFFF];
  }
  else
  {
    int final_addr = (bank << 14) | (addr14);

    return &ram[final_addr & ADDR_MASK];
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



  VM_p = vic_ram_read(0);
  c_ptr_p = vic_ram_read(0);
  c_ptr_bm_p = vic_ram_read(0);
}

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
  {
    VM= ((value >> 4) & 0xF) << 10; //1K block
    c_ptr = ((value >> 1) & 0x7) << 11; //2K block

    VM_p = vic_ram_read(VM);
    c_ptr_p = vic_ram_read(c_ptr);
    c_ptr_bm_p = vic_ram_read(c_ptr & 0x2000);

    //printf("Raster line %i\n",RASTER_Y);
  }
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
    mode =  ((value & 0x60) |  MCM) >> 4;
    break;
  case CTRL2:
#if 0
    printf("CSEL=%i XSCROLL=%i\n",CSEL,XSCROLL);

    if(CSEL) {
      first_x = 24;
      last_x = 343;
    } else {
      first_x = 31;
      last_x= 334;
    }
#endif
    mode = (ECM | BMM | (value & 0x10)) >> 4;
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

  mode = (ECM | BMM | MCM) >> 4;
}

uint8_t
vic_reg_read(uint16_t address)
{
  //printf("VIC reg read %4.4x line %i\n",address,RASTER_Y);
  switch (address)
  {
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
    //uint16_t MP = *vic_ram_read(VM | 0x3f8 | i)<<6; //8 bits
    uint16_t MP =VM_p[0x3f8 | i]<<6; //8 bits

    s->pixels =  *vic_ram_read( MP  | (s->MCBASE + 0)) << 24;
    s->pixels |= *vic_ram_read( MP  | (s->MCBASE + 1)) << 16;
    s->pixels |= *vic_ram_read( MP  | (s->MCBASE + 2)) << 8;

    // There are 4 accesses here, one data pointer access and 3 pixel data, two accesses pr cycle gives two cycles.
    stun_cycles+=2;
    s->MCBASE += 3;
    s->pixels = s->pixels >> (s->X & 7);
  }

  if ( s->pixels )
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
          color = 0xFF; //Transparent
          break;
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

        if(color !=0xFF) {
          *pixel++ = rgb_palette[color];
          *pixel++ = rgb_palette[color];
        } else {
          pixel+=2;
        }
        if(((vic_regs[MxXE] & (1<<i)) == 0) || (j&1)  ) {
          s->pixels <<= 2;
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

static void cpu_clock(int n) {
  static int budget = 0;

  budget+=n;

  while(budget>0) {
    int c = step6502();

    for(int j=0; j < c; j++) {
      sid_clock();
      //cia_clock();
    }

    budget -=c;
  }
}




/**
 * Lookup table for expanding 4 bits to 16bit
 */
unsigned int bit_map_lut[] = {
0x0000,0x1000,0x0100,0x1100,
0x0010,0x1010,0x0110,0x1110,
0x0001,0x1001,0x0101,0x1101,
0x0011,0x1011,0x0111,0x1111,
};


static inline void eight_pixels2c( uint32_t*p, uint8_t data,int color1, int color0) {
  uint32_t c = (bit_map_lut[data & 0xF]) <<16;
  c |= bit_map_lut[data>>4 ];
  uint32_t c1 = c*color0;
  uint32_t c0 = (c ^ 0x11111111)*color1;

  *p = (c1 | c0);
}


static inline void eight_pixels4c( uint32_t*p, uint8_t data,uint8_t color_lut[]) {
  uint32_t c;

  c  = color_lut[(data>>6) & 3];
  c |= color_lut[(data>>4) & 3]<<8;
  c |= color_lut[(data>>2) & 3]<<16;
  c |= color_lut[(data>>0) & 3]<<24;
  c |= c<<4;

  *p = c;
}



static inline void emit_border_pixels(uint32_t* p,int count) {
  int color0 = vic_regs[BO_COLOR] & 0xF;
  cpu_clock(count);
  for(int j=0; j <count; j++) {
    *p++ = 0x11111111*color0;
  }
}


#define MODE_FUN(n,core) \
static void mode##n(uint32_t* p,int VCBASE, int RC) { \
  for (int j = 0; j < 40; j++) { \
    int color0,color1; \
    uint8_t g_data; \
    int VC = VCBASE+j; \
\
  uint8_t D = video_ram[j] & 0xFF; \
  uint8_t bit_color = (video_ram[j] >> 8);  \
  core \
      cpu_clock(1); \
  } \
} \
static void mode_ba##n(uint32_t* p,int VCBASE, int RC) { \
  for (int j = 0; j < 40; j++) { \
    int color0,color1; \
    uint8_t g_data; \
    int VC = VCBASE+j; \
                    \
      video_ram[j] = ((color_ram[VCBASE + j] << 8) & 0xF00) | VM_p[ VCBASE + j]; \
\
  uint8_t D = video_ram[j] & 0xFF; \
  uint8_t bit_color = (video_ram[j] >> 8);  \
  core \
  sid_clock(); \
  } \
}


MODE_FUN(0,
    //Text mode
    //Read Pixels
    g_data = c_ptr_p[(D << 3) | RC];
    color1 = bit_color;
    color0 = vic_regs[BG_COLOR0];
    eight_pixels2c(p,g_data ,color0, color1); p++;
);


MODE_FUN(1,
    //Multicolor text mode
  g_data = c_ptr_p[(D << 3) | RC];
  uint8_t colors[4];
  colors[0] =vic_regs[BG_COLOR0];
  colors[1] =vic_regs[BG_COLOR1];
  colors[2] =vic_regs[BG_COLOR2];
  colors[3] =bit_color & 7;
  eight_pixels4c(p,g_data ,colors); p++;
);

MODE_FUN(2,
  //Bitmap mode
  g_data = c_ptr_bm_p[(VC << 3) | RC];

  color1 = (D >> 4) & 0xF;
  color0 = (D >> 0) & 0xF;
  eight_pixels2c(p,g_data ,color0, color1); p++;
);

MODE_FUN(3,
  //Multicolor Bitmap mode
  g_data = c_ptr_bm_p[(VC << 3) | RC];

  uint8_t colors[4];
  colors[0] =vic_regs[BG_COLOR0];
  colors[1] = (D >> 4) & 0xF;
  colors[2] = (D >> 0) & 0xF;
  colors[3] =bit_color & 0xF;

  eight_pixels4c(p,g_data ,colors); p++;
);

MODE_FUN(4,
    //ECM Text mode
    //Read Pixels
    g_data = c_ptr_p[(D << 3) | RC];
    color1 = bit_color;
    color0 = (D >> 6) & 3;;
    eight_pixels2c(p,g_data ,color0, color1); p++;
);

MODE_FUN(5,
    //ECM invalid
    eight_pixels2c(p,0 ,0, 0); p++;
);

typedef void (*core_fun_t)(uint32_t* p,int VCBASE, int RC);

core_fun_t mode_funs[] = {
    mode0,mode1,mode2,mode3,mode4,mode5,mode5,mode5,
};

core_fun_t mode_funs_ba[] = {
    mode_ba0,mode_ba1,mode_ba2,mode_ba3,mode_ba4,mode_ba5,mode_ba5,mode_ba5,
};


void
vic_line(int line, uint32_t* pixels_p)
{
  RASTER_Y = line;

  cia_clock(63);

  //Is this line within the 200 visible lines
  if (RASTER_Y >= first_line && RASTER_Y <= last_line)
  {
    //Bad line condition
    int RC = (line + YSCROLL) & 7;
    int BA = (RC == 0);
    int VCBASE = ((line + YSCROLL - first_line)/8) * 40;

    //Leading 12 cycles
    emit_border_pixels(pixels_p,11);

    if(BA) {
      mode_funs_ba[mode](pixels_p+ 11,VCBASE,RC);
    } else {
      mode_funs[mode](pixels_p+ 11,VCBASE,RC);
    }

//    mode0(pixels_p+ 11,BA,VCBASE,RC);
    //Lagging 12 cycles
    emit_border_pixels(pixels_p+(40+11),12);
  }
  else
  {
    //Full line
    emit_border_pixels(pixels_p,63);
  }

  //Interrupt handling
  if (RST == RASTER_Y)
  {
    vic_regs[IRQ] |= 0x01;
    if (vic_regs[IRQ_EN] & 1)
    {
      vic_regs[IRQ] |= 0x80;
      irq6502();
    }
  }

}

void vic_translate_line(int line,uint32_t* line_buf) {
  for(int i=0; i < 64; i++) {
    pixelbuf[line][8*i+0] = rgb_palette[ (line_buf[i] >>0) & 0xF ];
    pixelbuf[line][8*i+1] = rgb_palette[ (line_buf[i] >>4) & 0xF ];
    pixelbuf[line][8*i+2] = rgb_palette[ (line_buf[i] >>8) & 0xF ];
    pixelbuf[line][8*i+3] = rgb_palette[ (line_buf[i] >>12) & 0xF ];
    pixelbuf[line][8*i+4] = rgb_palette[ (line_buf[i] >>16) & 0xF ];
    pixelbuf[line][8*i+5] = rgb_palette[ (line_buf[i] >>20) & 0xF ];
    pixelbuf[line][8*i+6] = rgb_palette[ (line_buf[i] >>24) & 0xF ];
    pixelbuf[line][8*i+7] = rgb_palette[ (line_buf[i] >>28) & 0xF ];
  }
}

