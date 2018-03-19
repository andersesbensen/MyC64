/*
 * mysid.c
 *
 *  Created on: Feb 21, 2018
 *      Author: aes
 */

#include<stdint.h>
#include "envelopefp.h"

extern "C"
{
#include "mysid.h"
}

#include <stdio.h>
#include <iostream>

// SID registers

struct Voice
{
  uint8_t FreqLo; // Frequency: FreqLo/FreqHi
  uint8_t FreqHi; // Frequency: FreqLo/FreqHi
  uint8_t PWLo; // PulseWitdht: PW LO/HI only 12 bits used in SID
  uint8_t PWHi; // PulseWitdht: PW LO/HI only 12 bits used in SID
  uint8_t ControlReg; // NOISE,RECTANGLE,SAWTOOTH,TRIANGLE,TEST,RINGMOD,SYNC,GATE
  uint8_t AttackDecay; // bit0-3 decay, bit4-7 attack
  uint8_t SustainRelease; // bit0-3 release, bit4-7 sustain
};
struct Blocks
{
  struct Voice voice[3];
  uint8_t FCLo; // not implemented
  uint8_t FCHi; // not implemented

  uint8_t RES_Filt; // partly implemented
  uint8_t Mode_Vol; // partly implemented
  uint8_t POTX; // not implemented
  uint8_t POTY; // not implemented
  uint8_t OSC3_Random; // not implemented
  uint8_t ENV3; // not implemented
}__attribute__((packed));;

#define NUMREGISTERS 29

union Soundprocessor
{
  struct Blocks block;
  uint8_t sidregister[NUMREGISTERS];
} Sid;

// SID voice controll register bits
#define GATE (1<<0)
#define SYNC (1<<1)   // not implemented
#define RINGMOD (1<<2)  // implemented
#define TEST (1<<3)   // not implemented
#define TRIANGLE (1<<4)
#define SAWTOOTH (1<<5)
#define RECTANGLE (1<<6)
#define NOISE (1<<7)

// SID RES/FILT ( reg.23 )
#define FILT1 (1<<0)
#define FILT2 (1<<1)
#define FILT3 (1<<2)
// SID MODE/VOL ( reg.24 )
#define VOICE3OFF (1<<7)

#define SYS_CLOCK_HZ 1000000
#define SID_CLOCK_HZ 15625
#define OUTBITS 8
#define OUTMASK ((1<<OUTBITS)-1)

#define Freq 15625

static int b0;
static int b1;
static int b2;
static int a1;
static int a2;

#define NORM (1<<14)
uint16_t
biquad(uint16_t x0)
{
  static int x1, x2;
  static int y1, y2;

  uint16_t y0;

  y0 = (b0 * x0) / NORM + (b1 * x1) / NORM + (b2 * x2) / NORM - (a1 * y1) / NORM - (a2 * y2) / NORM;

  y2 = y1;
  y1 = y0;

  x2 = x1;
  x1 = x0;

  return y0;
}

#include<math.h>
#define Q 1.0
void
biquad_lp(double w0)
{
  b0 = (1 - cos(w0) / 2.0) * NORM;
  b1 = b0 * 2;
  b2 = b0;

  double alpha = sin(w0) / (2.0 * Q);
  double a0 = (1.0 + alpha);

  a1 = (-2 * cos(w0))/a0 * NORM;
  a2 = (1 - alpha)/a0 * NORM;


//  printf("Low pass %i %i %i %i %i\n",b0,b1,2,a1,a2);

}

void
biquad_hp(double w0)
{
  b0 = (1 + cos(w0) / 2.0) * NORM;
  b1 = NORM - b0 * 2;
  b2 = b0;

  double alpha = sin(w0) / (2.0 * Q);
  double a0 = (1.0 + alpha);

  a1 = (-2 * cos(w0))/a0 * NORM;
  a2 = (1 - alpha)/a0 * NORM;

//  printf("High pass %i %i %i %i %i\n",b0,b1,2,a1,a2);

}

void
biquad_bp(double w0)
{
  double alpha = sin(w0) / (2.0 * Q);

  b0 = alpha * NORM;
  b1 = 0;
  b2 = -alpha;

  double a0 = (1.0 + alpha);
  a1 = (-2 * cos(w0))/a0 * NORM;
  a2 = (1 - alpha)/a0 * NORM;


//  printf("Band pass %i %i %i %i %i\n",b0,b1,2,a1,a2);

}

class Oscillator
{
public:

  uint32_t phase;
  uint16_t fcw;
  uint16_t pw;
  uint16_t noise;
  uint8_t control;

  Oscillator()
  {
    phase = 0;
    fcw = 0;
    control = 0;
    noise = 0x64fe;
    pw =0;
  }

  uint8_t
  out()
  {
    phase += fcw;

    unsigned int saw_out = (phase >> 10);
    unsigned int tri_out = saw_out & 0x80 ? saw_out : (~saw_out);
    unsigned int rect_out = ((phase >> 6) & 0xfff) >= pw ? 0xffff : 0;

    int out = 0xff;

    if (phase & (1 << 13))
    {
      int bit0 = noise >> 15;
      noise = (noise << 1) | bit0;
    }

    if (control & SAWTOOTH)
      out &= saw_out;
    if (control & TRIANGLE)
      out &= tri_out;
    if (control & RECTANGLE)
      out &= rect_out;
    if (control & NOISE)
      out &= noise;

    return out & 0xff;

  }
};


EnvelopeGeneratorFP envelope[3];
Oscillator oscillator[3];

int16_t sid_buf[SAMPLE_BUFFER_SIZE];

static int ptr_n = 0;

extern "C"
{
  void
  sid_clock()
  {
    int direct_out = 0;
    int filter_out = 0;
    int osc1, osc2, osc3;

    for(int j=0; j < 3; j++) {
        envelope[j].clock(63);
    }

    osc1 = (oscillator[0].out() * envelope[0].readENV());
    osc2 = (oscillator[1].out() * envelope[1].readENV());
    osc3 = (oscillator[2].out() * envelope[2].readENV());

    if ((Sid.block.RES_Filt & FILT1))
    {
      filter_out += osc1;
    }
    else
    {
      direct_out += osc1;
    }

    if ((Sid.block.RES_Filt & FILT2))
    {
      filter_out += osc2;
    }
    else
    {
      direct_out += osc2;
    }

    if ((Sid.block.Mode_Vol & VOICE3OFF) == 0)
    {
      if ((Sid.block.RES_Filt & FILT3))
      {
          filter_out += osc3;
      }
      else
      {
        direct_out += osc3;
      }
    }
    //direct_out+=   filter_out ; //do_filter(filter_out,&filters[0] ) ;
    direct_out+=biquad(filter_out);
    sid_buf[ptr_n] = direct_out ;
    ptr_n++;

    if (ptr_n == SAMPLE_BUFFER_SIZE)
    {
     // std::cout << envelope[0].readENV() << std::endl;
      sid_audio_ready(sid_buf, SAMPLE_BUFFER_SIZE);
      ptr_n = 0;
    }

  }

  void
  sid_init()
  {
    //initialize SID-registers
    Sid.sidregister[6] = 0xF0;
    Sid.sidregister[13] = 0xF0;
    Sid.sidregister[20] = 0xF0;

    double w = (2*M_PI)*2000.0 / SID_CLOCK_HZ;

    biquad_lp(w);
  }

  uint8_t
  sid_read(uint16_t addr)
  {
    return 0xff;
  }





  void
  sid_write(uint16_t addr, uint8_t val)
  {
    Sid.sidregister[addr] = val;

    for (int i = 0; i < 3; i++)
    {
      oscillator[i].fcw = Sid.block.voice[i].FreqHi << 8 | Sid.block.voice[i].FreqLo << 0;
      oscillator[i].pw = (Sid.block.voice[i].PWHi << 8 | Sid.block.voice[i].PWLo << 0) & 0xfff;
      oscillator[i].control = Sid.block.voice[i].ControlReg;
    }


    int f = Sid.block.FCHi  << 8 |Sid.block.FCLo;
    double w = (2*M_PI)*f / SID_CLOCK_HZ;
    if(Sid.block.Mode_Vol & 0x10) biquad_lp(w);
    if(Sid.block.Mode_Vol & 0x20) biquad_bp(w);
    if(Sid.block.Mode_Vol & 0x30) biquad_hp(w);


    switch (addr) {
    case 0x04:
      envelope[0].writeCONTROL_REG(val);
      break;
    case 0x05:
      envelope[0].writeATTACK_DECAY( val);
      break;
    case 0x06:
      envelope[0].writeSUSTAIN_RELEASE(val);
      break;
    case 0x0b:
      envelope[1].writeCONTROL_REG(val);
      break;
    case 0x0c:
      envelope[1].writeATTACK_DECAY(val);
      break;
    case 0x0d:
      envelope[1].writeSUSTAIN_RELEASE(val  );
      break;
    case 0x12:
      envelope[2].writeCONTROL_REG(val);
      break;
    case 0x13:
      envelope[2].writeATTACK_DECAY( val);
      break;
    case 0x14:
      envelope[2].writeSUSTAIN_RELEASE(val);
      break;
    case 0x15:
    default:
      break;
    }
  }


}
