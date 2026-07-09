/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "FFT.h"
#include "ad9833.h"
#include "state.h"
#include "command.h"
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define SWEEP_AUTO_RUN            1U
#define SWEEP_PRINT_RESULT        0U

#define SWEEP_START_FREQ_HZ       100U
#define SWEEP_STOP_FREQ_HZ        3000U
#define SWEEP_STEP_FREQ_HZ        100U
#define SWEEP_SAMPLE_FREQ_HZ      20000U
#define SWEEP_ADC_SAMPLES         1024U
#define SWEEP_SETTLE_MS           20U
#define SWEEP_TIMEOUT_MS          200U
#define SWEEP_POINT_COUNT         (((SWEEP_STOP_FREQ_HZ - SWEEP_START_FREQ_HZ) / SWEEP_STEP_FREQ_HZ) + 1U)
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static volatile uint8_t adc_dma_done = 0U;

#if defined(__CC_ARM)
static uint16_t sweep_adc_buffer[SWEEP_ADC_SAMPLES] __attribute__((section("DMA_RAM"), zero_init));
#else
static uint16_t sweep_adc_buffer[SWEEP_ADC_SAMPLES] __attribute__((section(".dma_ram"), aligned(32)));
#endif
volatile uint32_t sweep_freq_result[SWEEP_POINT_COUNT];
volatile float sweep_fft_freq_result[SWEEP_POINT_COUNT];
volatile float sweep_amp_result[SWEEP_POINT_COUNT];
volatile uint8_t sweep_finished = 0U;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void Sweep_SetTim3SampleRate(uint32_t sample_rate_hz);
static HAL_StatusTypeDef Sweep_CaptureAdc(void);
static void Sweep_Process(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_TIM3_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
	HAL_ADCEx_Calibration_Start(&hadc1, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED);
  HAL_TIM_Base_Start(&htim3);
  My_Usart_Init();
  AD9833_Init_GPIO();
  State_Init();
//  AD9833_WaveSeting(1000,0,SIN_WAVE,0 );
//  AD9833_AmpSet(105);
//	for(int i=27;i<255;i++){
//		AD9833_AmpSet(i);
//		HAL_Delay(100);
//	}
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {   
    Usart_Rx_Proc();
#if SWEEP_AUTO_RUN
    Sweep_Process();
#else
    State_Proc();
#endif
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);
  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}
  /** Macro to configure the PLL clock source
  */
  __HAL_RCC_PLL_PLLSOURCE_CONFIG(RCC_PLLSOURCE_HSE);
  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 5;
  RCC_OscInitStruct.PLL.PLLN = 192;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
static uint32_t Sweep_GetTim3ClockHz(void)
{
  uint32_t tim_clock_hz = HAL_RCC_GetPCLK1Freq();

  if ((RCC->D2CFGR & RCC_D2CFGR_D2PPRE1) != RCC_D2CFGR_D2PPRE1_DIV1)
  {
    tim_clock_hz *= 2U;
  }

  return tim_clock_hz;
}

static void Sweep_SetTim3SampleRate(uint32_t sample_rate_hz)
{
  uint32_t tim_clock_hz;
  uint32_t ticks_per_sample;
  uint32_t prescaler;
  uint32_t period;

  if (sample_rate_hz == 0U)
  {
    return;
  }

  tim_clock_hz = Sweep_GetTim3ClockHz();
  ticks_per_sample = tim_clock_hz / sample_rate_hz;
  if (ticks_per_sample == 0U)
  {
    ticks_per_sample = 1U;
  }

  prescaler = (ticks_per_sample + 65535U) / 65536U;
  if (prescaler == 0U)
  {
    prescaler = 1U;
  }

  period = ticks_per_sample / prescaler;
  if (period == 0U)
  {
    period = 1U;
  }

  HAL_TIM_Base_Stop(&htim3);
  __HAL_TIM_SET_PRESCALER(&htim3, prescaler - 1U);
  __HAL_TIM_SET_AUTORELOAD(&htim3, period - 1U);
  __HAL_TIM_SET_COUNTER(&htim3, 0U);
  htim3.Instance->EGR = TIM_EGR_UG;

  FFT_SetSampling((float)(tim_clock_hz / (prescaler * period)));
}

static HAL_StatusTypeDef Sweep_CaptureAdc(void)
{
  uint32_t start_tick;

  adc_dma_done = 0U;

  SCB_InvalidateDCache_by_Addr((uint32_t *)sweep_adc_buffer, sizeof(sweep_adc_buffer));

  HAL_TIM_Base_Stop(&htim3);
  __HAL_TIM_SET_COUNTER(&htim3, 0U);

  if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)sweep_adc_buffer, SWEEP_ADC_SAMPLES) != HAL_OK)
  {
    return HAL_ERROR;
  }

  HAL_TIM_Base_Start(&htim3);

  start_tick = HAL_GetTick();
  while (adc_dma_done == 0U)
  {
    if ((HAL_GetTick() - start_tick) > SWEEP_TIMEOUT_MS)
    {
      HAL_TIM_Base_Stop(&htim3);
      HAL_ADC_Stop_DMA(&hadc1);
      return HAL_TIMEOUT;
    }
  }

  HAL_TIM_Base_Stop(&htim3);
  HAL_ADC_Stop_DMA(&hadc1);

  SCB_InvalidateDCache_by_Addr((uint32_t *)sweep_adc_buffer, sizeof(sweep_adc_buffer));

  return HAL_OK;
}

static void Sweep_Process(void)
{
  static uint32_t sweep_freq_hz = SWEEP_START_FREQ_HZ;
  uint32_t index;
  float fft_amp = 0.0f;

  index = (sweep_freq_hz - SWEEP_START_FREQ_HZ) / SWEEP_STEP_FREQ_HZ;

  AD9833_WaveSeting(sweep_freq_hz, 0, SIN_WAVE, 0);
  HAL_Delay(SWEEP_SETTLE_MS);

  Sweep_SetTim3SampleRate(SWEEP_SAMPLE_FREQ_HZ);

  if (Sweep_CaptureAdc() == HAL_OK)
  {
    FFT_Process(sweep_adc_buffer, &fft_amp);

    sweep_freq_result[index] = sweep_freq_hz;
    sweep_fft_freq_result[index] = FFT_GetFrequency();
    sweep_amp_result[index] = fft_amp;

#if SWEEP_PRINT_RESULT
    printf("%lu,%.2f,%.3f\r\n", sweep_freq_hz, sweep_fft_freq_result[index], sweep_amp_result[index]);
#endif
  }

  sweep_freq_hz += SWEEP_STEP_FREQ_HZ;
  if (sweep_freq_hz > SWEEP_STOP_FREQ_HZ)
  {
    sweep_freq_hz = SWEEP_START_FREQ_HZ;
    sweep_finished = 1U;
    HAL_Delay(1000U);
  }
  else
  {
    sweep_finished = 0U;
  }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
  if (hadc->Instance == ADC1)
  {
    adc_dma_done = 1U;
  }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
