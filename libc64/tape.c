/*
 * tape.c
 *
 *  Created on: Mar 4, 2017
 *      Author: aes
 */

/*
  Bytes: $0000-000B: File signature "C64-TAPE-RAW"
                 000C: TAP version (see below for description)
                        $00 - Original layout
                         01 - Updated
            000D-000F: Future expansion
            0010-0013: File  data  size  (not  including  this  header,  in
                       LOW/HIGH format) i.e. This image is $00082151  bytes
                       long.*/
#include<stdint.h>
#include<stdio.h>
#include<stdlib.h>

#include "cia.h"
static uint8_t* pulse_times;
static int idx=0;
static int cnt;
static int size=0;
typedef struct
{
  char signature[0xC];
  uint8_t version;
  char reserved[3];
  uint32_t size;
} tap_header_t;

static tap_header_t hdr;

void open_tape(const char* file) {
  FILE *f;

  f = fopen(file,"rb");
  fread(&hdr,sizeof(hdr),1,f);

  printf("File size is %x version is %i\n", hdr.size,hdr.version);
  pulse_times = malloc(hdr.size);

  size = hdr.size;

  fread(pulse_times,size,1,f);

  printf("first byte %i \n",pulse_times[0]);
  fclose(f);

  idx =0;

  cnt=1;
}

extern uint8_t ram[16*4096];
#define PROT_CASETTE_CTRL  0x20
#define PROT_CASETTE_SENSE 0x10

int tape_sense =0;
void tape_cycle(int n) {


  tape_sense=idx >= size;

  if(idx >= size) return;

  if(ram[1] & PROT_CASETTE_CTRL) return;

  for(int i=0; i < n; i++) {

  cnt--;

  if(cnt<=0) {
    cnt = pulse_times[idx++]*8;


    if((cnt ==0) && (hdr.version==1)) {
      cnt = ((pulse_times[idx+2] << 16) | (pulse_times[idx+1] << 8) | pulse_times[idx+0]) ;
      idx+=3;

      printf("Large count %08x-%08x %08x\n",idx,size,cnt);

    }

    if(cia1.IRQ_MASK & ( 1<<4 )) {
      cia1.IRQ |=  0x80 | 0x10;
      irq6502();
    }
    //printf("count %08x-%08x %08x\n",idx,size,cnt);

    /*if(cnt < 63*2) {
      printf("******* %i\n",cnt);
    }*/

  }
  }
}

