

#include "c64.h"
#include <stddef.h>

#include <stdbool.h>
#include "driverlib/systick.h"
#include "driverlib/timer.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/udma.h"

#include "inc/hw_memmap.h"
#include "inc/hw_gpio.h"
#include "inc/hw_types.h"

uint32_t frame_time;


void vic_screen_draw_done () {
}







int main() {

  //Setup core clock to 80Mhz
/*  SysCtlClockSet(SYSCTL_SYSDIV_2_5 | SYSCTL_USE_PLL | SYSCTL_XTAL_16MHZ);  //80MHz

  SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER2);
  TimerConfigure( TIMER2_BASE, TIMER_CFG_PERIODIC_UP);
  TimerEnable(TIMER2_BASE, TIMER_A);
*/
  //pal_init();
  //while(1) asm("wfe");

  c64_init();
  while(1) {
    int last_val = TimerValueGet(TIMER2_BASE,TIMER_A);

    c65_run_frame();
    int timer_val = TimerValueGet(TIMER2_BASE,TIMER_A);

    frame_time = (timer_val - last_val) / (SysCtlClockGet() / 1000);

  }
  return 0;
}
