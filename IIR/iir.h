#ifndef __IIR_H__
#define __IIR_H__

#include "stm32h7xx_hal.h"
#include "arm_math.h"
#include "math.h"
#include "stdio.h"
#include <stdint.h>
#include "HMI.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef PI
#define PI 3.14159265358979323846
#endif

#define SAMPLE_NUM      500U
#define MATRIX_SIZE     5U
#define VECTOR_SIZE     5U

// 这里填 IIR 实际运行采样率，不是扫频步长
#define Fs              1000000.0

#define EPS             1e-15
#define Q48_SCALE       35184372088831LL

typedef struct
{
    float64_t r;
    float64_t i;
} complex;

typedef struct
{
    float64_t b0;
    float64_t b1;
    float64_t b2;
    float64_t a1;
    float64_t a2;
} analog_coef;

typedef struct
{
    int64_t b0;
    int64_t b1;
    int64_t b2;
    int64_t a1;
    int64_t a2;
} digital_coef;

#define FILTER_MODE_THR_HIGH 0.6
#define FILTER_MODE_THR_LOW 0.3

typedef enum
{
    LOW_PASS_FILTER = 0,
    HIGH_PASS_FILTER,
    BAND_PASS_FILTER,
    BAND_STOP_FILTER
} FILTER_TYPE;

extern float freq_table[SAMPLE_NUM];

void coef_calc(const complex *sample_data);

FILTER_TYPE get_last_fit_filter_type(void);

void show_filter_type(FILTER_TYPE type);

analog_coef matrix_calc(void);

complex complex_div(const complex *data0, const complex *data1);

digital_coef bilinear_transform_quant(const analog_coef *analog_coef_data);

#ifdef __cplusplus
}
#endif

#endif
