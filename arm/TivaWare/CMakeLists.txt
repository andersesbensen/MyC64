
add_library(tivadrv 
driverlib/adc.c   driverlib/cpu.c     driverlib/emac.c   driverlib/gpio.c       driverlib/lcd.c      driverlib/qei.c     driverlib/sysctl.c   driverlib/uart.c
driverlib/aes.c   driverlib/crc.c     driverlib/epi.c    driverlib/hibernate.c  driverlib/mpu.c      driverlib/shamd5.c  driverlib/sysexc.c   driverlib/udma.c
driverlib/can.c   driverlib/des.c     driverlib/flash.c  driverlib/i2c.c        driverlib/onewire.c  driverlib/ssi.c     driverlib/systick.c  driverlib/usb.c
driverlib/comp.c  driverlib/eeprom.c  driverlib/fpu.c    driverlib/interrupt.c  driverlib/pwm.c      driverlib/sw_crc.c  driverlib/timer.c    driverlib/watchdog.c
)

target_include_directories(tivadrv PUBLIC . )