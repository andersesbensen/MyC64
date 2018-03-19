/*
 * cia.c
 *
 *  Created on: Feb 24, 2017
 *      Author: aes
 */
#include "cia.h"
#include "fake6502.h"
#include <string.h>
//#include<stdio.h>

extern int clock_tick;

//#define dbg_print(fmt,...) printf("%10i %s: " fmt,clock_tick,cia->name,__VA_ARGS__)

#define dbg_print(fmt,...)

ALL_STATIC cia_t cia1;
ALL_STATIC cia_t cia2;

uint8_t key_matrix2[8];


void cia_reg_write(cia_t* cia,uint16_t addr, uint8_t value) {

  switch(addr) {
  case 0:
    cia->PRA = value;
    if(cia == &cia2)vic_update_ptrs();
    break;
  case 1:
    cia->PRB = value;
    break;
  case 2:
    cia->DDRA= value;
    break;
  case 3:
    cia->DDRB=value;
    break;
  case 4:
    cia->t[0].latch &= 0xFF00;
    cia->t[0].latch |=value;
    break;
  case 5:
    cia->t[0].latch &= 0xFF;
    cia->t[0].latch |=((uint16_t)value<<8);
    break;
  case 6:
    cia->t[1].latch &= 0xFF00;
    cia->t[1].latch |=value;
    break;
  case 7:
    cia->t[1].latch &= 0xFF;
    cia->t[1].latch |=((uint16_t)value<<8);
    break;
  case 8:
  case 9:
  case 0xA:
  case 0xB: //TODO
  case 0xC:
    break;
  case 0xD:
    dbg_print("Write IRQ mask %x\n",value);
    if(value & 0x80) { //Setting mask
      cia->IRQ_MASK |= value & 0x1F;
    } else {          //Clear
      cia->IRQ_MASK &= ~(value & 0x1F);
    }

    break;
  case 0xE:
    dbg_print("Write TimerA Control\n",0);

    if(value & 0x10) {
      cia->t[0].value = cia->t[0].latch;
    }
    cia->t[0].control = (value & ~0x10);
    break;
  case 0xF:
    dbg_print("Write TimerB Control\n",0);

    if(value & 0x10) {
      cia->t[1].value = cia->t[1].latch;
    }
    cia->t[1].control = (value & ~0x10);
    break;
  }
}


uint8_t cia_reg_read(cia_t* cia, uint16_t addr) {

  switch(addr)
  {
  case 0:
    return cia->PRA ;
  case 1:
    if(cia == &cia1) {
      uint8_t key=0x0;
      for(int i=0; i < 8; i++) {
        if( (cia1.PRA & (1<<i)) == 0) {
          key |= key_matrix2[i];
        }
      }
      return 0xFF & ~key;
    } else {
      return cia->PRB;
    }
  case 2:
    return cia->DDRA;
  case 3:
    return cia->DDRB;
  case 4:
    return cia->t[0].value & 0xFF;
  case 5:
    return (cia->t[0].value>>8) & 0xFF;
  case 6:
    return cia->t[1].value & 0xFF;
  case 7:
    return (cia->t[1].value>>8) & 0xFF;
  case 8:
  case 9:
  case 0xA:
  case 0xB:
  case 0xC:
    //printf("Implement me\n");
    return 0; //TODO
  case 0xD: //Interrupt status
  {
    int val = cia->IRQ;
    dbg_print("CIA interrupt read %i clearing\n",val);
    cia->IRQ = 0;
    return val;
  }
  case 0xE:
    dbg_print("Read TimerA Control\n",0);
    return cia->t[0].control;
  case 0xF:
    dbg_print("Read TimerB Control\n",0);
    return cia->t[1].control;
  default:
    return 0;
  }
}

#include <stdio.h>
static int cia_clock_timer(cia_timer_t *t,int n) {
  if(t->control & 1) {
    if((t->control & 0x20)) //SP count or CNT
    {
      //TODO
      printf ("Timer TODO\n");
      t->value-=n;
    } else {
      t->value-=n;
    }

    if(t->value<=0) { //Underflow
      if((t->control & 8)==0) { //Reload
        //printf("Reload\n");
        t->value += t->latch;
      } else {
        t->control &= ~1; //Disable
      }
      return 1;
    }
  }
  return 0;
}

static void __cia_clock(cia_t* cia,int n) {

  if( cia_clock_timer(&cia->t[0],n )){
   dbg_print("TimerA underflow latch %i\n",cia->t[0].latch);

    cia->IRQ |= 1;
    if(cia->IRQ_MASK & 1) {
      cia->IRQ |= 0x80;

      if(cia == &cia1) {
        irq6502();
      } else {
        nmi6502();
      }
    }
  }

  if(cia_clock_timer(&cia->t[1],n)){
    dbg_print("TimerB underflow latch %i\n",cia->t[1].latch);

    cia->IRQ |= 2;
    if(cia->IRQ_MASK & 2) {
      cia->IRQ |= 0x80;

      if(cia == &cia1) {
        irq6502();
      } else {
        nmi6502();
      }

    }
  }
}

void cia_clock(int n) {
  __cia_clock(&cia1,n);
  __cia_clock(&cia2,n);
}

void cia_init() {
  memset(&cia1, 0, sizeof(cia_t));
  memset(&cia2, 0, sizeof(cia_t));
  cia1.name = "CIA1";
  cia2.name = "CIA2";
}

