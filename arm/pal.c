/*
 * pal.c
 *
 *  Created on: Mar 18, 2017
 *      Author: aes
 */
#define PART_TM4C1230H6PM

#include<stdint.h>
#include<stdbool.h>

#include "driverlib/systick.h"
#include "driverlib/timer.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/udma.h"
#include "driverlib/interrupt.h"
#include "driverlib/udma.h"

#include "inc/hw_memmap.h"
#include "inc/hw_gpio.h"
#include "inc/hw_timer.h"
#include "inc/hw_ints.h"
#include "inc/hw_udma.h"

//Drives signal from 0 - 0.3V
#define SYNC_PIN     (1<<2)
//Drives signal from 0 - 0.3V
#define SYNC_PIN     (1<<2)

//Drives signal from 0.3 - 1V
#define LUMINA_PINS  (3<<3)

#include "inc/hw_types.h"

#define CLOCK_MHZ 80
#define DMA_TRANSFER_SIZE 32
/*DMA transmit buffers */
uint8_t luma_bufferA[DMA_TRANSFER_SIZE];
uint8_t luma_bufferB[DMA_TRANSFER_SIZE];


uint8_t* luma_buffer;
static int dma_transfer_count;

tDMAControlTable DMAcontroltable[64] __attribute__ ((aligned(1024)));



//Convert nano secondes to clock cycles
#define NS_TO_CLOCK(x) ( ((x) * CLOCK_MHZ) /1000  )

const int pal_timings_cycles[] =
  { NS_TO_CLOCK(1650), //FRONT_PORTH_A
  NS_TO_CLOCK(4700), //SYNC_PULSE_B
  NS_TO_CLOCK(5700), //BACK_PORCH_C
  NS_TO_CLOCK(51950), //ACTIVE_VIDEO
  NS_TO_CLOCK(2000), //SHORT_PULSE_ON
  NS_TO_CLOCK(30000), //SHORT_PULSE_OFF
  NS_TO_CLOCK(30000), //LONG_PULSE_ON
  NS_TO_CLOCK(2000), //LONG_PULSE_OFF
    };


/**
 * Set sync value
 */
inline void sync(int v) {
 if(v){
   HWREG(GPIO_PORTA_BASE + (GPIO_O_DATA + (SYNC_PIN << 2))) = SYNC_PIN;
 } else {
   HWREG(GPIO_PORTA_BASE + (GPIO_O_DATA + (SYNC_PIN << 2))) = 0;
 }
}

/**
 * Set new timeout
 */
inline void set_timeout(int clocks) {
  //TimerLoadSet(TIMER0_BASE,TIMER_B, clocks );  //Divide sysclock by 10 should now be 8Mhz
  HWREG(TIMER0_BASE + TIMER_O_TAILR) =clocks;
  HWREG(TIMER0_BASE + TIMER_O_CTL) |=TIMER_CTL_TAEN;
}



typedef struct {
  int n; //Number of time to repeat this pulse
  int off_cycles; //Number of on cycles
  int on_cycles; //Number of off cycles
} pulse_t;

#define N_PULSES 4 //4 is non-interlaced 8 is interlaced
pulse_t pulses[N_PULSES] = {
    {305, 4*CLOCK_MHZ,60*CLOCK_MHZ},
      {5, 2*CLOCK_MHZ,30*CLOCK_MHZ},
      {5,30*CLOCK_MHZ, 2*CLOCK_MHZ},
      {4, 2*CLOCK_MHZ,30*CLOCK_MHZ},
    {306, 4*CLOCK_MHZ,60*CLOCK_MHZ},
      {6, 2*CLOCK_MHZ,30*CLOCK_MHZ},
      {5,30*CLOCK_MHZ, 2*CLOCK_MHZ},
      {5, 2*CLOCK_MHZ,30*CLOCK_MHZ},
};

int sync_val =0;
int sync_pulse=0;
int n_pulses =0;
void pal_sync_timout() {
  HWREG(TIMER0_BASE + TIMER_O_ICR) = 0xFFFFFFFF;

  sync_val = !sync_val;
  sync(sync_val);

  if(sync_val) {
    if(sync_pulse == 0) {
      TimerEnable(TIMER1_BASE, TIMER_A);
    }

    n_pulses--;

    if(n_pulses==0) {
      if(sync_pulse ==(N_PULSES-1)) {
        sync_pulse=0;
      } else {
        sync_pulse++;
      }
      n_pulses = pulses[sync_pulse].n;
    }

    set_timeout(pulses[sync_pulse].on_cycles);
  } else {
    set_timeout(pulses[sync_pulse].off_cycles);
  }
}

enum {DMA_BUFFER_PRIMARY,DMA_BUFFER_ALTERNATE} dma_buffer;

/**
 * Send out our luma codes
 */
inline void dma_transfer_start() {
  //Setup transfer pointers, and transfer size

  if(dma_buffer == DMA_BUFFER_PRIMARY) {
    DMAcontroltable[0].pvSrcEndAddr = luma_bufferA+DMA_TRANSFER_SIZE -1;
    DMAcontroltable[0].ui32Control =
        UDMA_SIZE_8 | UDMA_SRC_INC_8 | UDMA_DST_INC_NONE | UDMA_ARB_1 |
        UDMA_MODE_PINGPONG | ((DMA_TRANSFER_SIZE - 1) << 4);

    dma_buffer = DMA_BUFFER_ALTERNATE;
    luma_buffer = luma_bufferB;
  } else {
    DMAcontroltable[32].pvSrcEndAddr = luma_bufferB+DMA_TRANSFER_SIZE -1;
    DMAcontroltable[32].ui32Control =
        UDMA_SIZE_8 | UDMA_SRC_INC_8 | UDMA_DST_INC_NONE | UDMA_ARB_1 |
        UDMA_MODE_PINGPONG | ((DMA_TRANSFER_SIZE - 1) << 4);
    dma_buffer = DMA_BUFFER_PRIMARY;
    luma_buffer = luma_bufferA;
  }
}

void dma_done() {
  if(dma_transfer_count) {
    dma_transfer_start();
    dma_transfer_count--;
  } else {
    uDMAChannelDisable(UDMA_CH0_TIMER4A);
  }
  //Clear interrupt
  HWREG(TIMER4_BASE + TIMER_O_ICR) = TIMER_TIMA_DMA;
}

void pal_line_start() {
  HWREG(TIMER1_BASE + TIMER_O_ICR) = 0xFFFFFFFF;

  dma_buffer = DMA_BUFFER_PRIMARY;
  dma_transfer_start();
  //Start the DMA transfer
  HWREG(UDMA_ENASET) = 1 << (UDMA_CH0_TIMER4A & 0x1f);
  TimerLoadSet(TIMER4_BASE,TIMER_A, 10 );  //Divide sysclock by 10 should now be 8Mhz

  dma_transfer_count = 11;//55/(DMA_TRANSFER_SIZE/8)-1;
}

void pal_init() {
  SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
  SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER1);

  SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER4);
  SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
  SysCtlPeripheralEnable(SYSCTL_PERIPH_UDMA);

  IntMasterEnable();

  /* Timer0 is used for sync signal */
  TimerConfigure( TIMER0_BASE, TIMER_CFG_A_ONE_SHOT);
  TimerIntEnable(TIMER0_BASE,TIMER_TIMA_TIMEOUT);
  IntEnable(INT_TIMER0A_TM4C123);
  TimerEnable(TIMER0_BASE, TIMER_A);

  TimerIntRegister(TIMER0_BASE,TIMER_A,pal_sync_timout);

  /* Timer1 is used to start picture data transfer */
  TimerConfigure( TIMER1_BASE, TIMER_CFG_A_ONE_SHOT);
  TimerIntEnable(TIMER1_BASE,TIMER_TIMA_TIMEOUT);
  IntEnable(INT_TIMER1A);

  TimerLoadSet(TIMER1_BASE, TIMER_A, NS_TO_CLOCK(5700) ); //The backporch time
  TimerIntRegister(TIMER1_BASE,TIMER_A,pal_line_start);


  //Set the SYNC pin and lumina pins to output
  GPIODirModeSet(GPIO_PORTA_BASE,SYNC_PIN|LUMINA_PINS,GPIO_DIR_MODE_OUT);
  GPIOPadConfigSet(GPIO_PORTA_BASE,SYNC_PIN|LUMINA_PINS,GPIO_STRENGTH_10MA, GPIO_PIN_TYPE_STD);
  GPIOPinWrite(GPIO_PORTA_BASE,LUMINA_PINS,LUMINA_PINS);


  //Timer4 A is used for the 8MHZ pixel clock
  TimerConfigure( TIMER4_BASE, TIMER_CFG_A_PERIODIC);
  TimerLoadSet(TIMER4_BASE,TIMER_A, 10 );  //Divide sysclock by 10 should now be 8Mhz
  TimerDMAEventSet(TIMER4_BASE,TIMER_DMA_TIMEOUT_A); //Setup DMA event
  TimerEnable(TIMER4_BASE, TIMER_A);

  //funny enough the timer interrupt is used when the dma transfer is done
  TimerIntClear(TIMER4_BASE,TIMER_TIMA_DMA);
  TimerIntRegister(TIMER4_BASE,TIMER_A,dma_done);
  TimerIntEnable(TIMER4_BASE,TIMER_TIMA_DMA);

  //Setup DMA controller
  uDMAEnable();
  uDMAControlBaseSet(DMAcontroltable);

  //DMA channel 0 is triggered by timer 4A
  uDMAChannelAssign(UDMA_CH0_TIMER4A);

  uDMAChannelAttributeDisable(UDMA_CH0_TIMER4A,
      UDMA_ATTR_ALTSELECT | UDMA_ATTR_USEBURST |
      UDMA_ATTR_HIGH_PRIORITY | UDMA_ATTR_REQMASK);

  // -8bits pr transfer
  // - increment source by one on each transfer
  // - Don't increment destination
  // - arbitration size is 1 element
  uDMAChannelControlSet(UDMA_CH0_TIMER4A | UDMA_PRI_SELECT,
  UDMA_SIZE_8 | UDMA_SRC_INC_8 | UDMA_DST_INC_NONE | UDMA_ARB_1);
  uDMAChannelControlSet(UDMA_CH0_TIMER4A | UDMA_ALT_SELECT,
  UDMA_SIZE_8 | UDMA_SRC_INC_8 | UDMA_DST_INC_NONE | UDMA_ARB_1);


  //Setup the DMA destination pointer since this does not change
  DMAcontroltable[0].pvDstEndAddr = (void *)(GPIO_PORTA_BASE + GPIO_O_DATA +(LUMINA_PINS << 2));
  DMAcontroltable[32].pvDstEndAddr = (void *)(GPIO_PORTA_BASE + GPIO_O_DATA +(LUMINA_PINS << 2));


  //Setup our double buffers
  luma_buffer = luma_bufferB;

  /* Make a test picture */
  for(int i=0 ; i < DMA_TRANSFER_SIZE-1; i++) {
    luma_bufferA[i] = ((3-i) & 3) <<3  ;
    luma_bufferB[i] = (i & 3) <<3  ;
  }

  //memset(luma_bufferB,3<<3, sizeof(luma_bufferB));
  /*for(int i=0 ; i < DMA_TRANSFER_SIZE-1; i++) {
    luma_bufferB[i] = (1)<<3;
  }*/

  set_timeout(100);
  n_pulses = 1;

}


