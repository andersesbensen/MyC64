/*
 * fake6502.h
 *
 *  Created on: Feb 24, 2017
 *      Author: aes
 */

#ifndef TESTAPPLICATIONS_MYC64_FAKE6502_H_
#define TESTAPPLICATIONS_MYC64_FAKE6502_H_
#include <stdint.h>
/**
 *    - Call this once before you begin execution.    *
 *
 */
  void reset6502();
/**
 *  *   - Execute 6502 code up to the next specified    *
 *     count of clock ticks.                         *
 *
 */
void exec6502(uint32_t tickcount) ;

/*
 * *   - Execute a single instrution.                  *
 *  Return the numner of clocks spent                  *
 */
 int step6502();

 /*

 *   - Trigger a hardware IRQ in the 6502 core.      *
   *                                                   *
  */
 void irq6502();

 /*
 *   - Trigger an NMI in the 6502 core.              *
 *                                                   *
 *                                                   */
void nmi6502() ;

/*
 *  *   - Pass a pointer to a void function taking no   *
 *     parameters. This will cause Fake6502 to call  *
 *     that function once after each emulated        *
 *     instruction.                                  *
 *                                                   *
 */
 void hookexternal(void *funcptr) ;

 /*
  *  *   - A running total of the emulated cycle count.  *
  *
  */
 extern uint32_t clockticks6502;

 /*
  *  *   - A running total of the total emulated         *
 *     instruction count. This is not related to     *
 *     clock cycle timing.                           *
 *
 */
 extern int32_t instructions;

/**
 * Fake6502 requires you to provide two external     *
 * functions:                                        *
 */
extern uint8_t read6502(uint16_t address);
extern void write6502(uint16_t address, uint8_t value);

#endif /* TESTAPPLICATIONS_MYC64_FAKE6502_H_ */
