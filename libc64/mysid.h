
#define SAMPLE_RATE 15625
#define SAMPLE_BUFFER_SIZE (8*1024)


void sid_audio_ready(int16_t* data, int size);
void sid_init();
uint8_t sid_read(uint16_t addr) ;
void sid_write(uint16_t addr, uint8_t val);
void sid_clock();

int sid_get_data(uint16_t* data, int n);