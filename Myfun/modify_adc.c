#include "adc.h"
#include "dma.h"
#include "main.h"
#include "modify_adc.h"

extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern DMA_HandleTypeDef hdma_adc1;
extern DMA_HandleTypeDef hdma_adc2;

volatile adc_mode_ctrl_t g_adc_mode_ctrl =
{
    .mode = ADC_MODE_LEARN,
    .adc1_done = 0U,
    .adc2_done = 0U,
    .adc_flag = 0U,
    .iir_process_flags = 0U
};

void App_ADC_SetMode(adc_mode_t mode)
{
    g_adc_mode_ctrl.mode = mode;
}

void App_ADC_ResetFlags(void)
{
    g_adc_mode_ctrl.adc1_done = 0U;
    g_adc_mode_ctrl.adc2_done = 0U;
    g_adc_mode_ctrl.adc_flag = 0U;
    g_adc_mode_ctrl.iir_process_flags = 0U;
}

/* 进入滤波模式：ADC1 改成 circular，只采 PA0 */
uint8_t App_ADC1_Reconfig_ForFilter(uint16_t *adc_buf, uint32_t adc_len)
{

    if ((adc_buf == NULL) || (adc_len == 0U))
    {
        return 0U;
    }

    (void)HAL_ADC_Stop_DMA(&hadc1);
    (void)HAL_ADC_Stop_DMA(&hadc2);

    (void)HAL_DMA_DeInit(&hdma_adc1);

    hdma_adc1.Instance = DMA1_Stream0;   /* 按你当前工程的 ADC1 stream */
    hdma_adc1.Init.Request = DMA_REQUEST_ADC1;
    hdma_adc1.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_adc1.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_adc1.Init.MemInc = DMA_MINC_ENABLE;
    hdma_adc1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_adc1.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_adc1.Init.Mode = DMA_CIRCULAR;
    hdma_adc1.Init.Priority = DMA_PRIORITY_HIGH;
    hdma_adc1.Init.FIFOMode = DMA_FIFOMODE_DISABLE;

    if (HAL_DMA_Init(&hdma_adc1) != HAL_OK)
    {
        return 0U;
    }

    __HAL_LINKDMA(&hadc1, DMA_Handle, hdma_adc1);

    hadc1.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DMA_CIRCULAR;
    hadc1.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;

    MODIFY_REG(hadc1.Instance->CFGR, ADC_CFGR_DMNGT, ADC_CONVERSIONDATA_DMA_CIRCULAR);
    MODIFY_REG(hadc1.Instance->CFGR, ADC_CFGR_OVRMOD, ADC_OVR_DATA_OVERWRITTEN);

    if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_buf, adc_len) != HAL_OK)
    {
        return 0U;
    }

    App_ADC_ResetFlags();
    App_ADC_SetMode(ADC_MODE_FILTER);

    return 1U;
}

/* 回到学习模式：ADC1、ADC2 都改回 one-shot + normal */
uint8_t App_ADC_Restore_ForLearn(uint16_t *adc1_buf, uint16_t *adc2_buf, uint32_t adc_len)
{
    if ((adc1_buf == NULL) || (adc2_buf == NULL) || (adc_len == 0U))
    {
        return 0U;
    }

    (void)HAL_ADC_Stop_DMA(&hadc1);
    (void)HAL_ADC_Stop_DMA(&hadc2);

    /* ADC1 */
    (void)HAL_DMA_DeInit(&hdma_adc1);
    hdma_adc1.Instance = DMA1_Stream0;
    hdma_adc1.Init.Request = DMA_REQUEST_ADC1;
    hdma_adc1.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_adc1.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_adc1.Init.MemInc = DMA_MINC_ENABLE;
    hdma_adc1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_adc1.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_adc1.Init.Mode = DMA_NORMAL;
    hdma_adc1.Init.Priority = DMA_PRIORITY_LOW;
    hdma_adc1.Init.FIFOMode = DMA_FIFOMODE_DISABLE;

    if (HAL_DMA_Init(&hdma_adc1) != HAL_OK)
    {
        return 0U;
    }

    __HAL_LINKDMA(&hadc1, DMA_Handle, hdma_adc1);

    hadc1.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DMA_ONESHOT;
    hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;

    MODIFY_REG(hadc1.Instance->CFGR, ADC_CFGR_DMNGT, ADC_CONVERSIONDATA_DMA_ONESHOT);
    MODIFY_REG(hadc1.Instance->CFGR, ADC_CFGR_OVRMOD, ADC_OVR_DATA_PRESERVED);

    /* ADC2 */
    (void)HAL_DMA_DeInit(&hdma_adc2);
    hdma_adc2.Instance = DMA1_Stream1;
    hdma_adc2.Init.Request = DMA_REQUEST_ADC2;
    hdma_adc2.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_adc2.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_adc2.Init.MemInc = DMA_MINC_ENABLE;
    hdma_adc2.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_adc2.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_adc2.Init.Mode = DMA_NORMAL;
    hdma_adc2.Init.Priority = DMA_PRIORITY_LOW;
    hdma_adc2.Init.FIFOMode = DMA_FIFOMODE_DISABLE;

    if (HAL_DMA_Init(&hdma_adc2) != HAL_OK)
    {
        return 0U;
    }

    __HAL_LINKDMA(&hadc2, DMA_Handle, hdma_adc2);

    hadc2.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DMA_ONESHOT;
    hadc2.Init.Overrun = ADC_OVR_DATA_PRESERVED;

    MODIFY_REG(hadc2.Instance->CFGR, ADC_CFGR_DMNGT, ADC_CONVERSIONDATA_DMA_ONESHOT);
    MODIFY_REG(hadc2.Instance->CFGR, ADC_CFGR_OVRMOD, ADC_OVR_DATA_PRESERVED);

    if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc1_buf, adc_len) != HAL_OK)
    {
        return 0U;
    }

    if (HAL_ADC_Start_DMA(&hadc2, (uint32_t *)adc2_buf, adc_len) != HAL_OK)
    {
        return 0U;
    }

    App_ADC_ResetFlags();
    App_ADC_SetMode(ADC_MODE_LEARN);

    return 1U;
}

