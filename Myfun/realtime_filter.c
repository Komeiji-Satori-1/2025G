#include "realtime_filter.h"

#include "adc.h"
#include "dac.h"
#include "tim.h"
#include "calculate.h"

#define RT_BUFFER_LEN              ADC_LEN
#define RT_HALF_BUFFER_LEN         (ADC_LEN / 2U)
#define RT_ADC_MID_CODE            32768.0f
#define RT_DAC_MID_CODE            2048.0f
#define RT_ADC_TO_DAC_SCALE        (4095.0f / 65535.0f)

ALIGN_32BYTES(static uint16_t rt_adc_buf[RT_BUFFER_LEN]);
ALIGN_32BYTES(static uint16_t rt_dac_buf[RT_BUFFER_LEN]);

static volatile uint8_t realtime_running = 0U;

static float rt_b0 = 0.0f;
static float rt_b1 = 0.0f;
static float rt_b2 = 0.0f;
static float rt_a1 = 0.0f;
static float rt_a2 = 0.0f;

static float rt_x1 = 0.0f;
static float rt_x2 = 0.0f;
static float rt_y1 = 0.0f;
static float rt_y2 = 0.0f;

static void RealtimeFilter_ResetState(void)
{
    rt_x1 = 0.0f;
    rt_x2 = 0.0f;
    rt_y1 = 0.0f;
    rt_y2 = 0.0f;
}

static void RealtimeFilter_FillDacBuffer(uint16_t value)
{
    for (uint32_t i = 0U; i < RT_BUFFER_LEN; i++)
    {
        rt_dac_buf[i] = value;
    }
}

static uint16_t RealtimeFilter_ClampToDac(float value)
{
    if (value <= 0.0f)
    {
        return 0U;
    }

    if (value >= 4095.0f)
    {
        return 4095U;
    }

    return (uint16_t)(value + 0.5f);
}

static void RealtimeFilter_LoadCoefficients(void)
{
    const digital_coef *coef = calculate_get_digital_coef();
    const double scale = (double)Q48_SCALE;

    if (coef == NULL)
    {
        rt_b0 = 0.0f;
        rt_b1 = 0.0f;
        rt_b2 = 0.0f;
        rt_a1 = 0.0f;
        rt_a2 = 0.0f;
        return;
    }

    rt_b0 = (float)((double)coef->b0 / scale);
    rt_b1 = (float)((double)coef->b1 / scale);
    rt_b2 = (float)((double)coef->b2 / scale);
    rt_a1 = (float)((double)coef->a1 / scale);
    rt_a2 = (float)((double)coef->a2 / scale);
}

static HAL_StatusTypeDef RealtimeFilter_ApplyAdc1Mode(uint8_t circular)
{
    ADC1_SetDmaCircularMode(circular);

    if (HAL_ADC_DeInit(&hadc1) != HAL_OK)
    {
        return HAL_ERROR;
    }

    MX_ADC1_Init();

    if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED) != HAL_OK)
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}

static void RealtimeFilter_ProcessSample(uint32_t index)
{
    float x;
    float y;
    float dac_value;

    x = (float)rt_adc_buf[index] - RT_ADC_MID_CODE;
    y = (rt_b0 * x) + (rt_b1 * rt_x1) + (rt_b2 * rt_x2) - (rt_a1 * rt_y1) - (rt_a2 * rt_y2);

    rt_x2 = rt_x1;
    rt_x1 = x;
    rt_y2 = rt_y1;
    rt_y1 = y;

    dac_value = RT_DAC_MID_CODE + (y * RT_ADC_TO_DAC_SCALE);
    rt_dac_buf[index] = RealtimeFilter_ClampToDac(dac_value);
}

uint8_t RealtimeFilter_IsRunning(void)
{
    return realtime_running;
}

void RealtimeFilter_Init(void)
{
    realtime_running = 0U;
    RealtimeFilter_ResetState();
    RealtimeFilter_FillDacBuffer((uint16_t)RT_DAC_MID_CODE);
}

uint8_t RealtimeFilter_Start(void)
{
    if (!calculate_iir_coeff_ready())
    {
        printf("Realtime filter start failed: IIR coefficients are not ready.\r\n");
        return 0U;
    }

    if (realtime_running != 0U)
    {
        RealtimeFilter_Stop();
    }

    if (HAL_TIM_Base_Stop(&htim8) != HAL_OK)
    {
        printf("Realtime filter warning: failed to stop TIM8.\r\n");
    }

    if (HAL_ADC_Stop_DMA(&hadc1) != HAL_OK)
    {
        printf("Realtime filter warning: failed to stop ADC1 DMA.\r\n");
    }

    if (HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1) != HAL_OK)
    {
        printf("Realtime filter warning: failed to stop DAC1 DMA.\r\n");
    }

    if (RealtimeFilter_ApplyAdc1Mode(1U) != HAL_OK)
    {
        printf("Realtime filter start failed: ADC1 reconfiguration error.\r\n");
        (void)RealtimeFilter_ApplyAdc1Mode(0U);
        RealtimeFilter_FillDacBuffer((uint16_t)RT_DAC_MID_CODE);
        return 0U;
    }

    RealtimeFilter_LoadCoefficients();
    RealtimeFilter_ResetState();
    RealtimeFilter_FillDacBuffer((uint16_t)RT_DAC_MID_CODE);

    if (HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, (uint32_t *)rt_dac_buf, RT_BUFFER_LEN, DAC_ALIGN_12B_R) != HAL_OK)
    {
        printf("Realtime filter start failed: DAC1 DMA start error.\r\n");
        (void)HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1);
        (void)RealtimeFilter_ApplyAdc1Mode(0U);
        RealtimeFilter_FillDacBuffer((uint16_t)RT_DAC_MID_CODE);
        return 0U;
    }

    if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)rt_adc_buf, RT_BUFFER_LEN) != HAL_OK)
    {
        printf("Realtime filter start failed: ADC1 DMA start error.\r\n");
        (void)HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1);
        (void)HAL_ADC_Stop_DMA(&hadc1);
        (void)RealtimeFilter_ApplyAdc1Mode(0U);
        RealtimeFilter_FillDacBuffer((uint16_t)RT_DAC_MID_CODE);
        return 0U;
    }

    __HAL_TIM_SET_COUNTER(&htim8, 0U);
    realtime_running = 1U;

    if (HAL_TIM_Base_Start(&htim8) != HAL_OK)
    {
        printf("Realtime filter start failed: TIM8 start error.\r\n");
        realtime_running = 0U;
        (void)HAL_ADC_Stop_DMA(&hadc1);
        (void)HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1);
        (void)RealtimeFilter_ApplyAdc1Mode(0U);
        RealtimeFilter_FillDacBuffer((uint16_t)RT_DAC_MID_CODE);
        return 0U;
    }

    printf("Realtime filter started.\r\n");
    return 1U;
}

void RealtimeFilter_Stop(void)
{
    if (realtime_running == 0U)
    {
        return;
    }

    if (HAL_TIM_Base_Stop(&htim8) != HAL_OK)
    {
        printf("Realtime filter warning: failed to stop TIM8.\r\n");
    }

    if (HAL_ADC_Stop_DMA(&hadc1) != HAL_OK)
    {
        printf("Realtime filter warning: failed to stop ADC1 DMA.\r\n");
    }

    if (HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1) != HAL_OK)
    {
        printf("Realtime filter warning: failed to stop DAC1 DMA.\r\n");
    }

    if (RealtimeFilter_ApplyAdc1Mode(0U) != HAL_OK)
    {
        printf("Realtime filter warning: failed to restore ADC1 oneshot mode.\r\n");
    }

    RealtimeFilter_FillDacBuffer((uint16_t)RT_DAC_MID_CODE);
    RealtimeFilter_ResetState();
    realtime_running = 0U;
    printf("Realtime filter stopped.\r\n");
}

void RealtimeFilter_ProcessHalf(uint32_t offset)
{
    uint32_t end;

    if (realtime_running == 0U)
    {
        return;
    }

    if (offset >= RT_BUFFER_LEN)
    {
        return;
    }

    end = offset + RT_HALF_BUFFER_LEN;
    if (end > RT_BUFFER_LEN)
    {
        return;
    }

    for (uint32_t i = offset; i < end; i++)
    {
        RealtimeFilter_ProcessSample(i);
    }
}
