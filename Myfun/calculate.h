#include "FFT.h"
#include "modify_adc.h"

float calculate_vin(float vout, uint32_t f);
void calculate_set_ad9833_amp_by_vin(float vin);
uint8_t calculate_ad9833_amp_code(float vin);
uint8_t get_learn_done(void);