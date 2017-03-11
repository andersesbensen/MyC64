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

static uint8_t* pulse_times;
static uint8_t idx;
static int cnt;
static int data;

typedef struct
{
  char signature[0xB];
  uint8_t version;
  char reserved[3];
  uint32_t size;
} tap_header_t;


void open_tape(const char* file) {
  FILE *f;
  tap_header_t hdr;

  f = fopen(file,"rb");
  fread(&hdr,sizeof(hdr),1,f);

  printf("File size is %x\n", hdr.size);
  pulse_times = malloc(hdr.size);

  fread(&pulse_times,hdr.size,1,f);

  fclose(f);


  idx =0;
  data = 0;
  cnt=1;
}


void tape_cycle() {
  cnt--;

  if(cnt==0) {
    data = !data;
    cnt = pulse_times[idx++];
    if(cnt ==0) {
      cnt = pulse_times[idx]<<16 | pulse_times[idx+1]<<8 | pulse_times[idx+2];
      idx+=3;
    }
    cnt = cnt * 8;
  }
}

int tape_data() {
  return data;
}

