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

void vic_line(int line, uint32_t* pixels_p);
void vic_update_ptrs();

#endif /* TESTAPPLICATIONS_MYC64_VICII_H_ */
