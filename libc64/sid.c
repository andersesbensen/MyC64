/*
 * sid.c
 *
 *  Created on: Apr 19, 2017
 *      Author: aes
 */




#include <stdint.h>
/**
 * http://www.waitingforfriday.com/?p=661
 */

#define SYS_CLK_HZ 985248
#define SAMPLE_RATE_HZ 44100


typedef struct {
  enum {ENVELOPE_IDLE, ENVELOPE_ATTACK,ENVELOPE_DELAY,ENVELOPE_SUSTAN,ENVELOPE_RELESE} state;
  int8_t attack;
  int8_t delay;
  int8_t sustain;
  int8_t release;

  int envelope; //output value
  int a1; //Filter parameter
} envelop_gen_t;


typedef struct {
  /** Together these registers form a 16-bit number which linearly controls the Frequency of Oscillator  */
  uint16_t freq;

  /** Together these registers form a 12-bit number (bits 4-7 of PW Hi are not used) which linearly controls the Pulse Width (duty cycle) of the Pulse waveform on Oscillator*/
  uint16_t pulse_width;

  /** This register contains eight control bits which select various options on Oscillator */
  uint8_t ctrl;

  /** Attack/Decay */
  uint8_t attak_decay;

  /** Sustain/Release (Register 06) */
  uint8_t sustain_release;




  /* Oscillator counter  */
  uint16_t osc_cnt1;
  uint16_t osc_cnt2;

  /* Oscillator state */
  int osc_state;

  envelop_gen_t envelope;
} voice_t;
s
typedef struct {
  voice_t voice[3];

  /** ogether these registers form an 11-bit number (bits 3-7 of FC LO are not used) which linearly controls the Cutoff
   * (or Center) Frequency of the programmable Filter. The approximate Cutoff Frequency ranges between 30Hz and 10KHz
   * with the recommended capacitor values of 2200pF for CAP1 and CAP2.*/
  uint16_t cutoff;

/**
 * Bits 4-7 of this register (RES0-RES3) control the Resonance of the Filler,
 * resonance of a peaking effect which emphasizes frequency components at the Cutoff Frequency of the Filter,
 * causing a sharper sound. There are 16 Resonance settings ranging linearly from no resonance (0)
 * to maximum resonance (15 or #F).
 * Bits 0-3 determine which signals will be routed through the Filter:
 */
  uint8_t  resonance;

  uint8_t mode_vol;
} sid_t;


static int tone_generator(voice_t* v) {
  v->osc_cnt1--;
  v->osc_cnt2--;

  if(v->osc_cnt1==0) {
    v->osc_cnt1 = v->freq;
    v->osc_cnt2 = v->pulse_width;
    v->osc_state = 1;
  }
  if(v->osc_cnt2==0) {
    v->osc_state = 0;
  }

  return v->osc_state;
}

static int envelope_generator(voice_t* v) {

}


void sid_reg_write(cia_t* cia ,uint16_t addr, uint8_t value);
uint8_t cia_reg_read(cia_t* cia, uint16_t addr);






int16_t cia_create_sample() {

  return 0;
}

