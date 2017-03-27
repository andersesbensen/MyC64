/*
 * c64.h
 *
 *  Created on: Feb 26, 2017
 *      Author: aes
 */

#ifndef TESTAPPLICATIONS_MYC64_C64_H_
#define TESTAPPLICATIONS_MYC64_C64_H_


#ifndef ALL_STATIC
#define ALL_STATIC
#endif

#include<stdint.h>


#ifdef __arm__
#define NUM_4K_BLOCKS 6
#else
#define NUM_4K_BLOCKS 16
#endif

#define ADDR_MASK (NUM_4K_BLOCKS*4096-1)


void c64_init();
void c65_run_frame();

/**
 * @param matrix key code, upper byte is row and lower is column
 */
void c64_key_press(int key, int state);


/**
 * Load program into memory, Just type RUN to execute
 */
void c64_load_prg(const char* file);

extern uint32_t pixelbuf[312][512];


extern void vic_screen_draw_done();

#endif /* TESTAPPLICATIONS_MYC64_C64_H_ */
