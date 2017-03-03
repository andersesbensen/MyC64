/*
 * c64.h
 *
 *  Created on: Feb 26, 2017
 *      Author: aes
 */

#ifndef TESTAPPLICATIONS_MYC64_C64_H_
#define TESTAPPLICATIONS_MYC64_C64_H_

#include<stdint.h>

void c64_init();
void c65_run_frame();
void c64_key_press(char key, int state);

extern uint32_t pixelbuf[403][512];

#endif /* TESTAPPLICATIONS_MYC64_C64_H_ */
