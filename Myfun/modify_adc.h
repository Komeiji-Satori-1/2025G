#ifndef __MODIFY_ADC_H__
#define __MODIFY_ADC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

typedef enum
{
    ADC_MODE_LEARN = 0U,
    ADC_MODE_FILTER = 1U
} adc_mode_t;

typedef struct
{
    adc_mode_t mode;
    uint8_t adc1_done;
    uint8_t adc2_done;
    uint8_t adc_flag;
    uint8_t iir_adc_half_ready;
    uint8_t iir_adc_full_ready;
    uint8_t iir_dac_half_ready;
    uint8_t iir_dac_full_ready;
    uint32_t iir_overrun_count;
    uint32_t iir_adc_half_irq_count;
    uint32_t iir_adc_full_irq_count;
    uint32_t iir_dac_half_irq_count;
    uint32_t iir_dac_full_irq_count;
    uint32_t iir_dac_error_count;
} adc_mode_ctrl_t;

extern volatile adc_mode_ctrl_t g_adc_mode_ctrl;

void App_ADC_SetMode(adc_mode_t mode);
void App_ADC_ResetFlags(void);

uint8_t App_ADC1_Reconfig_ForFilter(uint16_t *adc_buf, uint32_t adc_len);
uint8_t App_ADC_Restore_ForLearn(uint16_t *adc1_buf, uint16_t *adc2_buf, uint32_t adc_len);

#ifdef __cplusplus
}
#endif

#endif /* __APP_ADC_RECONFIG_H__ */
