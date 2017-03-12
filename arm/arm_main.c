

#include "c64.h"
#include <stddef.h>
void vic_screen_draw_done () {

}
int main() {

  while(1) {
    c65_run_frame();
  }
  return 0;
}


void __aeabi_memclr(void *dest, size_t n) {

  uint8_t* c = (uint8_t*)dest;
  for(int i=0; i<n ; i++) {
    *c++ = 0;
  }
}

//void __aeabi_memclr4(uint32_t n, n: usize)
