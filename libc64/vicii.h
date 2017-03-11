/*
 * vicii.h
 *
 *  Created on: Feb 24, 2017
 *      Author: aes
 */

#ifndef TESTAPPLICATIONS_MYC64_VICII_H_
#define TESTAPPLICATIONS_MYC64_VICII_H_

#include<stdint.h>

void vic_init();
void    vic_reg_write(uint16_t address, uint8_t value);
uint8_t vic_reg_read(uint16_t address);

/**
 * Return the number of stunn cycles
 */
int vic_clock();

/**
 * Read a 14 bit address
 */
extern uint8_t vic_ram_read(uint16_t address);



#endif /* TESTAPPLICATIONS_MYC64_VICII_H_ */
