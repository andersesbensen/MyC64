/*
 * cia.h
 *
 *  Created on: Feb 24, 2017
 *      Author: aes
 */

#ifndef TESTAPPLICATIONS_MYC64_CIA_H_
#define TESTAPPLICATIONS_MYC64_CIA_H_

#include <stdint.h>
typedef struct {
  uint16_t value;
  uint16_t latch;
  uint8_t control;
} cia_timer_t;

typedef struct  {
  uint8_t PRA;
  uint8_t PRB;
  uint8_t DDRA;
  uint8_t DDRB;

  uint8_t TA_LO;
  uint8_t TA_HI;
  uint8_t TB_LO;
  uint8_t TB_HI;

  uint8_t TOD_10THS;
  uint8_t TOD_SEC;
  uint8_t TOD_MIN;
  uint8_t TOD_HR;

  uint8_t SDR;
  uint8_t ICR;

  uint8_t CRA;
  uint8_t CRB;

}  __attribute__((packed)) cia_registers_t;


typedef struct {
  const char* name;

  uint8_t PRA;
  uint8_t PRB;
  uint8_t DDRA; /* If a bit in the DDR is set to the corresponding bit in the PR is an output */
  uint8_t DDRB;


  cia_timer_t t[2];
  uint8_t IRQ_MASK;
  uint8_t IRQ;
} cia_t;


void cia_reg_write(cia_t* cia ,uint16_t addr, uint8_t value);
uint8_t cia_reg_read(cia_t* cia, uint16_t addr);

/**
 * Return true if edge interrupt occured.
 */
int cia_clock(cia_t* cia);

#endif /* TESTAPPLICATIONS_MYC64_CIA_H_ */
