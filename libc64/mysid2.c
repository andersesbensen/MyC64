/*
 * mysid.c
 *
 *  Created on: Feb 21, 2018
 *      Author: aes
 */

#include<stdint.h>

#include "mysid.h"


// SID registers

struct Voice
{
  uint16_t fcw;
  uint16_t pw;
  uint8_t ControlReg; // NOISE,RECTANGLE,SAWTOOTH,TRIANGLE,TEST,RINGMOD,SYNC,GATE
  uint8_t AttackDecay; // bit0-3 decay, bit4-7 attack
  uint8_t SustainRelease; // bit0-3 release, bit4-7 sustain
}__attribute__((packed));
;

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
}__attribute__((packed));
;

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

#define NORM (1<<14)

typedef enum
{
  ATTACK, DECAY_SUSTAIN, RELEASE
} envelope_state_t;

struct VoiceInternal
{
  uint32_t phase;
  uint16_t noise;
  int msb_latch;


  int16_t rate_counter;
  int16_t rate_period;
  uint8_t exponential_counter;
  uint8_t exponential_counter_period;
  uint8_t envelope_counter;

  envelope_state_t state;

  int hold_zero;
  int muted;
  int gate;
};

static struct VoiceInternal voice_int[3];

static void
update_rate_period(struct VoiceInternal* vi, int newperiod);
static void
envelope_reset(struct VoiceInternal* vi);

const uint8_t sustain_level[] =
  { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, };

const int16_t rate_counter_period[] =
  { 9, //   2ms*1.0MHz/256 =     7.81
      32, //   8ms*1.0MHz/256 =    31.25
      63, //  16ms*1.0MHz/256 =    62.50
      95, //  24ms*1.0MHz/256 =    93.75
      149, //  38ms*1.0MHz/256 =   148.44
      220, //  56ms*1.0MHz/256 =   218.75
      267, //  68ms*1.0MHz/256 =   265.63
      313, //  80ms*1.0MHz/256 =   312.50
      392, // 100ms*1.0MHz/256 =   390.63
      977, // 250ms*1.0MHz/256 =   976.56
      1954, // 500ms*1.0MHz/256 =  1953.13
      3126, // 800ms*1.0MHz/256 =  3125.00
      3907, //   1 s*1.0MHz/256 =  3906.25
      11720, //   3 s*1.0MHz/256 = 11718.75
      19532, //   5 s*1.0MHz/256 = 19531.25
      31251 //   8 s*1.0MHz/256 = 31250.00
    };

// ----------------------------------------------------------------------------
// Register functions.
// ----------------------------------------------------------------------------
static void
writeCONTROL_REG(const struct Voice* v, struct VoiceInternal* vi)
{
  int gate_next = v->ControlReg & 0x01;

  //printf("v->ControlReg %i",v->ControlReg);
  // The rate counter is never reset, thus there will be a delay before the
  // envelope counter starts counting up (attack) or down (release).

  // Gate bit on: Start attack, decay, sustain.
  if (!vi->gate && gate_next)
  {
    vi->state = ATTACK;
    update_rate_period(vi, rate_counter_period[v->AttackDecay >> 4]);
    // Switching to attack state unlocks the zero freeze.
    vi->hold_zero = 0;
  }
  // Gate bit off: Start release.
  else if (vi->gate && !gate_next)
  {
    vi->state = RELEASE;
    update_rate_period(vi, rate_counter_period[v->SustainRelease & 0xf]);
  }

  vi->gate = gate_next;
}

static void
writeATTACK_DECAY(const struct Voice* v, struct VoiceInternal * vi)
{

  if (vi->state == ATTACK)
  {
    update_rate_period(vi, rate_counter_period[v->AttackDecay >> 4]);
  }
  else if (vi->state == DECAY_SUSTAIN)
  {
    update_rate_period(vi, rate_counter_period[v->AttackDecay & 0xf]);
  }
}

void
writeSUSTAIN_RELEASE(const struct Voice* v, struct VoiceInternal* vi)
{
  if (vi->state == RELEASE)
  {
    update_rate_period(vi, rate_counter_period[v->SustainRelease & 0xf]);
  }
}

static void
update_rate_period(struct VoiceInternal* vi, int newperiod)
{
  vi->rate_period = newperiod;

  /* The ADSR counter is XOR shift register with 0x7fff unique values.
   * If the rate_period is adjusted to a value already seen in this cycle,
   * the register will wrap around. This is known as the ADSR delay bug.
   *
   * To simplify the hot path calculation, we simulate this through observing
   * that we add the 0x7fff cycle delay by changing the rate_counter variable
   * directly. This takes care of the 99 % common case. However, playroutine
   * could make multiple consequtive rate_period adjustments, in which case we
   * need to cancel the previous adjustment. */

  /* if the new period exeecds 0x7fff, we need to wrap */
  if (vi->rate_period - vi->rate_counter > 0x7fff)
    vi->rate_counter += 0x7fff;

  /* simulate 0x7fff wraparound, if the period-to-be-written
   * is less than the current value. */
  if (vi->rate_period <= vi->rate_counter)
    vi->rate_counter -= 0x7fff;

  /* at this point it should be impossible for
   * rate_counter >= rate_period. If it is, there is a bug... */
}

static void
envelope_reset(struct VoiceInternal* vi)
{
  vi->envelope_counter = 0;

  vi->gate = 0;

  vi->rate_counter = 0;
  vi->exponential_counter = 0;
  vi->exponential_counter_period = 1;

  vi->state = RELEASE;
  vi->rate_period = rate_counter_period[0];
  vi->hold_zero = 1;
}

static void
envelope_clock(const struct Voice* v, struct VoiceInternal* vi, int n)
{

  vi->rate_counter -= n;
  if (vi->rate_counter > vi->rate_period)
    return;

  vi->rate_counter = 0;

  // The first envelope step in the attack state also resets the exponential
  // counter. This has been verified by sampling ENV3.
  //
  if ((vi->state == ATTACK) || (++vi->exponential_counter == vi->exponential_counter_period))
  {
    vi->exponential_counter = 0;

    // Check whether the envelope counter is frozen at zero.
    if (vi->hold_zero)
    {
      return;
    }

    switch (vi->state)
    {
    case ATTACK:
      // The envelope counter can flip from 0xff to 0x00 by changing state to
      // release, then to attack. The envelope counter is then frozen at
      // zero; to unlock this situation the state must be changed to release,
      // then to attack. This has been verified by sampling ENV3.
      //
      ++vi->envelope_counter ;
      if (vi->envelope_counter == 0xff)
      {
        vi->state = DECAY_SUSTAIN;
        update_rate_period(vi, rate_counter_period[v->AttackDecay & 0xf]);
      }
      break;
    case DECAY_SUSTAIN:
      if (vi->envelope_counter != sustain_level[v->SustainRelease >> 4])
      {
        --vi->envelope_counter;
      }
      break;
    case RELEASE:
      // The envelope counter can flip from 0x00 to 0xff by changing state to
      // attack, then to release. The envelope counter will then continue
      // counting down in the release state.
      // This has been verified by sampling ENV3.
      // NB! The operation below requires two's complement integer.
      //
      --vi->envelope_counter ;
      break;
    }

    // Check for change of exponential counter period.
    switch (vi->envelope_counter)
    {
    case 0xff:
      vi->exponential_counter_period = 1;
      break;
    case 0x5d:
      vi->exponential_counter_period = 2;
      break;
    case 0x36:
      vi->exponential_counter_period = 4;
      break;
    case 0x1a:
      vi->exponential_counter_period = 8;
      break;
    case 0x0e:
      vi->exponential_counter_period = 16;
      break;
    case 0x06:
      vi->exponential_counter_period = 30;
      break;
    case 0x00:
      vi->exponential_counter_period = 1;

      // When the envelope counter is changed to zero, it is frozen at zero.
      // This has been verified by sampling ENV3.
      vi->hold_zero = 1;
      break;
    }
  }
}

uint8_t
voice_new_wave(const struct Voice* v, struct VoiceInternal*vi)
{
  uint32_t new_phase;

  new_phase = vi->phase + v->fcw;
  vi->msb_latch = ((new_phase ^ vi->phase)>>18) & 1;
  vi->phase = new_phase;
  unsigned int saw_out = (vi->phase >> 10);
  unsigned int tri_out = saw_out & 0x80 ? saw_out : (~saw_out);
  unsigned int rect_out = ((vi->phase >> 6) & 0xfff) >= v->pw ? 0xffff : 0;

  int out = 0xff;

  if (vi->phase & (1 << 13))
  {
    int bit0 = vi->noise >> 15;
    vi->noise = (vi->noise << 1) | bit0;
  }

  if (v->ControlReg & SAWTOOTH)
    out &= saw_out;
  if (v->ControlReg & TRIANGLE)
    out &= tri_out;
  if (v->ControlReg & RECTANGLE)
    out &= rect_out;
  if (v->ControlReg & NOISE)
    out &= vi->noise;

  return out;
}


static void
voice_sync(const struct Voice* v1, struct VoiceInternal*vi1,const struct Voice* v2, struct VoiceInternal*vi2)
{
  if((v1->ControlReg & SYNC) && (vi2->msb_latch) ) {
    vi1->phase = 0;
  }
}


int16_t sid_buf[SAMPLE_BUFFER_SIZE];

static int ptr_n = 0;

int sid_get_data(uint16_t* data, int n) {
  int samples_to_copy = n;
  if(samples_to_copy > ptr_n) samples_to_copy = ptr_n;

  memcpy( data, sid_buf, samples_to_copy*sizeof(uint16_t) );

  if(samples_to_copy < ptr_n) {
    memmove(&sid_buf[0], &sid_buf[samples_to_copy], (ptr_n-samples_to_copy) * sizeof(uint16_t)  );
    ptr_n -=samples_to_copy;
  } else {
    ptr_n = 0;
  }
  return samples_to_copy;
}

static int do_filter(int in) {
  static double y=0.0;
  const double a=0.1;
  y = (in*a + (1.0-a)*y);

  return (int)y*1.2;
}
void
sid_clock()
{
  long long  direct_out = 0;
  long long filter_out = 0;
  static int ring;
  for (int j = 0; j < 3; j++)
  {
    int wave;

    envelope_clock(&Sid.block.voice[j], &voice_int[j], 63);

    wave = voice_new_wave(&Sid.block.voice[j], &voice_int[j]);
    if((Sid.block.voice[j].ControlReg & 0x34) == 0x14) wave ^= ring ;

    ring = (voice_int[j].phase >>10) & 0x80;


    wave = (wave-127) * voice_int[j].envelope_counter;

    if(j==2) {
      voice_sync(&Sid.block.voice[j],&voice_int[j],&Sid.block.voice[j+1],&voice_int[j+1]);
    } else{
      voice_sync(&Sid.block.voice[j],&voice_int[j],&Sid.block.voice[0],&voice_int[0]);
    }


    if ((j == 2) && (Sid.block.Mode_Vol & VOICE3OFF))
      continue;
    if ((Sid.block.RES_Filt >> j) & 1)
    {
      filter_out += wave;
    }
    else
    {
      direct_out += wave;
    }
  }

//  direct_out+=   do_filter(filter_out,&filters[0] ) ;
//    direct_out+=biquad(filter_out);
//  direct_out+=   do_filter(filter_out);
  sid_buf[ptr_n] = (direct_out/3) + (do_filter(filter_out/3));
  ptr_n++;

  if (ptr_n == SAMPLE_BUFFER_SIZE)
  {
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

  envelope_reset(&voice_int[0]);
  envelope_reset(&voice_int[1]);
  envelope_reset(&voice_int[2]);

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
    writeCONTROL_REG(&Sid.block.voice[i], &voice_int[i]);
    writeATTACK_DECAY(&Sid.block.voice[i], &voice_int[i]);
    writeSUSTAIN_RELEASE(&Sid.block.voice[i], &voice_int[i]);
  }

}

