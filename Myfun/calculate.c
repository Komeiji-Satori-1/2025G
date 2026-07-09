#include "calculate.h"
#include "AD9833.h"
#include "iir.h"

extern void Start_ADC_Capture(void);
extern volatile uint8_t ADC_Flag;
extern uint16_t ADC1_IN[ADC_LEN ] ;
extern uint16_t ADC2_OUT[ADC_LEN ];
#define VIN_AMP_TABLE_SIZE (sizeof(vin_amp_table) / sizeof(vin_amp_table[0]))

#define LEARN_START_FREQ_HZ 1000U
#define LEARN_STOP_FREQ_HZ 50000U
#define LEARN_STEP_FREQ_HZ 200U
#define LEARN_SETTLE_MS 20U

#define LEARN_POINT_NUM  (((LEARN_STOP_FREQ_HZ - LEARN_START_FREQ_HZ) / LEARN_STEP_FREQ_HZ) + 1U)

float freq_table[LEARN_POINT_NUM];
static complex h_table[LEARN_POINT_NUM];
static analog_coef analog_coef_data;
static digital_coef digital_coef_data;
typedef enum
{
    LEARN_IDLE = 0,
    LEARN_SET_FREQ,
    LEARN_WAIT_STABLE,
    LEARN_START_ADC,
    LEARN_WAIT_ADC,
    LEARN_PROCESS_FFT,
    LEARN_NEXT_FREQ,
    LEARN_CALC_IIR,
    LEARN_DONE,
} LearnState_t;

typedef struct
{
    LearnState_t state;
    uint32_t freq;
    uint32_t wait_tick;
    uint16_t index;
    uint8_t running;
} LearnCtrl_t;

static LearnCtrl_t learn = {0};

typedef struct
{
    float vin;
    uint8_t code;
} VinAmpCodeTable_t;

float vout = 2.0f;
uint32_t f = 1000.0f;
float H_jw = 5.0f;
float vin = 0.0f;

static const VinAmpCodeTable_t vin_amp_table[] =
    {
        {0.0322f, 1},
        {0.0542f, 2},
        {0.0762f, 3},
        {0.1f, 4},
        {0.124f, 5},
        {0.147f, 6},
        {0.172f, 7},
        {0.195f, 8},
        {0.218f, 9},
        {0.242f, 10},
        {0.264f, 11},
        {0.289f, 12},
        {0.311f, 13},
        {0.333f, 14},
        {0.355f, 15},
        {0.376f, 16},
        {0.388f, 17},
        {0.416f, 18},
        {0.438f, 19},
        {0.460f, 20},
        {0.484f, 21},
        {0.510f, 22},
        {0.532f, 23},
        {0.554f, 24},
        {0.574f, 25},
        {0.598f, 26},
        {0.620f, 27},
        {0.642f, 28},
        {0.663f, 29},
        {0.684f, 30},
        {0.708f, 31},
        {0.732f, 32},
        {0.752f, 33},
        {0.774f, 34},
        {0.796f, 35},
        {0.828f, 36},
        {0.850f, 37},
        {0.872f, 38},
        {0.900f, 39},
        {0.920f, 40},
        {0.940f, 41},
        {0.964f, 42},
        {0.988f, 43},
        {1.02f, 44},
        {1.04f, 45},
        {1.06f, 46},
        {1.08f, 47},
        {1.10f, 48},
        {1.12f, 49},
        {1.15f, 50},
        {1.16f, 51},
        {1.18f, 52},
        {1.21f, 53},
        {1.23f, 54},
        {1.26f, 55},
        {1.28f, 56},
        {1.30f, 57},
        {1.32f, 58},
        {1.34f, 59},
        {1.36f, 60},
        {1.4f, 61},
        {1.41f, 62},
        {1.43f, 63},
        {1.46f, 64},
        {1.48f, 65},
        {1.50f, 66},
        {1.52f, 67},
        {1.54f, 68},
        {1.56f, 69},
        {1.58f, 70},
        {1.61f, 71},
        {1.65f, 72},
        {1.69f, 73},
        {1.71f, 74},
        {1.73f, 75},
        {1.75f, 76},
        {1.78f, 77},
        {1.80f, 78},
        {1.82f, 79},
        {1.84f, 80},
        {1.87f, 81},
        {1.89f, 82},
        {1.90f, 83},
        {1.93f, 84},
        {1.95f, 85},
        {1.97f, 86},
        {2.00f, 87},
        {2.02f, 88},
        {2.04f, 89},
        {2.06f, 90},
        {2.08f, 91},
        {2.11f, 92},
        {2.13f, 93},
        {2.15f, 94},
        {2.18f, 95},
        {2.20f, 96},
        {2.22f, 97},
        {2.24f, 98},
        {2.26f, 99},
        {2.28f, 100},
        {2.30f, 101},
        {2.33f, 102},
        {2.35f, 103},
        {2.38f, 104},
        {2.40f, 105},
        {2.42f, 106},
        {2.47f, 107},
        {2.50f, 108},
        {2.505f, 109},
        {2.51f, 110},
        {2.53f, 111},
        {2.55f, 112},
        {2.58f, 113},
        {2.60f, 114},
        {2.62f, 115},
        {2.64f, 116},
        {2.67f, 117},
        {2.69f, 118},
        {2.71f, 119},
        {2.73f, 120},
        {2.75f, 121},
        {2.77f, 122},
        {2.80f, 123},
        {2.82f, 124},
        {2.84f, 125},
        {2.86f, 126},
        {2.88f, 127},
        {2.91f, 128},
        {2.93f, 129},
        {2.96f, 130},
        {2.98f, 131},
        {3.00f, 132},
        {3.02f, 133},
        {3.04f, 134},
        {3.07f, 135},
        {3.09f, 136},
        {3.11f, 137},
        {3.13f, 138},
        {3.15f, 139},
        {3.18f, 140},
        {3.20f, 141},
        {3.22f, 142},
        {3.24f, 143},
        {3.26f, 144},
        {3.29f, 145},
        {3.31f, 146},
        {3.33f, 147},
        {3.35f, 148},
        {3.37f, 149},
        {3.39f, 150},
        {3.42f, 151},
        {3.44f, 152},
        {3.46f, 153},
        {3.48f, 154},
        {3.51f, 155},
        {3.53f, 156},
        {3.55f, 157},
        {3.58f, 158},
        {3.59f, 159},
        {3.62f, 160},
        {3.64f, 161},
        {3.66f, 162},
        {3.69f, 163},
        {3.70f, 164},
        {3.73f, 165},
        {3.75f, 166},
        {3.77f, 167},
        {3.79f, 168},
        {3.82f, 169},
        {3.84f, 170},
        {3.86f, 171},
        {3.89f, 172},
        {3.91f, 173},
        {3.93f, 174},
        {3.95f, 175},
        {3.97f, 176},
        {3.99f, 177},
        {4.02f, 178},
        {4.05f, 179},
        {4.08f, 180},
        {4.10f, 181},
        {4.12f, 182},
        {4.16f, 183},
        {4.18f, 184},
        {4.20f, 185},
        {4.22f, 186},
        {4.24f, 187},
        {4.26f, 188},
        {4.28f, 189},
        {4.30f, 190},
        {4.34f, 191},
        {4.36f, 192},
        {4.40f, 193},
        {4.42f, 194},
        {4.44f, 195},
        {4.46f, 196},
        {4.48f, 197},
        {4.50f, 198},
        {4.52f, 199},
        {4.53f, 200},
        {4.54f, 201},
        {4.56f, 202},
        {4.58f, 203},
        {4.62f, 204},
        {4.64f, 205},
        {4.66f, 206},
        {4.70f, 207},
        {4.72f, 208},
        {4.74f, 209},
        {4.76f, 210},
        {4.78f, 211},
        {4.80f, 212},
        {4.82f, 213},
        {4.84f, 214},
        {4.88f, 215},
        {4.89f, 216},
        {4.91f, 217},
        {4.94f, 218},
        {4.96f, 219},
        {4.98f, 220},
        {5.00f, 221},
        {5.02f, 222},
        {5.04f, 223},
        {5.07f, 224},
        {5.10f, 225},
        {5.12f, 226},
        {5.14f, 227},
        {5.16f, 228},
        {5.18f, 229},
        {5.21f, 230},
        {5.22f, 231},
        {5.24f, 232},
        {5.27f, 233},
        {5.3f, 234},
        {5.32f, 235},
        {5.33f, 236},
        {5.36f, 237},
        {5.38f, 238},
        {5.40f, 239},
        {5.42f, 240},
        {5.44f, 241},
        {5.46f, 242},
        {5.48f, 243},
        {5.50f, 244},
        {5.52f, 245},
        {5.54f, 246},
        {5.57f, 247},
        {5.60f, 248},
        {5.63f, 249},
        {5.65f, 250},
        {5.66f, 251},
        {5.68f, 252},
        {5.70f, 253},
        {5.72f, 254},
        {5.74f, 255},
};

float calculate_vin(float vout, uint32_t f)
{

    uint32_t f2 = f * f; // f^2
    // (1 - 3.94784 * 10^-7 * f^2)
    float term1 = 1.0f - 3.94784e-7f * f2;
    //(1.88496 * 10^-3 * f)
    float term2 = 1.88496e-3f * f;
    H_jw = 5.0f / sqrtf(term1 * term1 + term2 * term2);

    vin = vout / H_jw;

    return vin;
}

static uint8_t clamp_amp_code(float code)
{
    if (code <= 0.0f)
    {
        return 0;
    }

    if (code >= 255.0f)
    {
        return 255;
    }

    return (uint8_t)(code);
}

uint8_t calculate_ad9833_amp_code(float vin)
{
    uint16_t i;
    float vin0;
    float vin1;
    float code0;
    float code1;
    float ratio;
    float code;

    if (vin <= vin_amp_table[0].vin)
    {
        return vin_amp_table[0].code;
    }

    if (vin >= vin_amp_table[VIN_AMP_TABLE_SIZE - 1].vin)
    {
        return vin_amp_table[VIN_AMP_TABLE_SIZE - 1].code;
    }

    for (i = 0; i < VIN_AMP_TABLE_SIZE - 1; i++)
    {
        vin0 = vin_amp_table[i].vin;
        vin1 = vin_amp_table[i + 1].vin;

        if ((vin >= vin0) && (vin <= vin1))
        {
            code0 = (float)vin_amp_table[i].code;
            code1 = (float)vin_amp_table[i + 1].code;

            ratio = (vin - vin0) / (vin1 - vin0);
            code = code0 + ratio * (code1 - code0);

            return clamp_amp_code(code);
        }
    }

    return vin_amp_table[VIN_AMP_TABLE_SIZE - 1].code;
}

void calculate_set_ad9833_amp_by_vin(float vin)
{
    uint8_t amp_code;

    amp_code = calculate_ad9833_amp_code(vin);
    AD9833_AmpSet(amp_code);
}

void calculate_learn_start(void)
{
    learn.running = 1;
    learn.state = LEARN_SET_FREQ;
    learn.freq = LEARN_START_FREQ_HZ;
    learn.index = 0;
}

void calculate_learn_proc(void)
{
    switch (learn.state)
    {
    case LEARN_IDLE:
        break;

    case LEARN_SET_FREQ:
        AD9833_WaveSeting(learn.freq, 0, SIN_WAVE, 0);
        learn.wait_tick = HAL_GetTick(); // 计时函数
        learn.state = LEARN_WAIT_STABLE;
        break;

    case LEARN_WAIT_STABLE:
        if (HAL_GetTick() - learn.wait_tick >= LEARN_SETTLE_MS) // 判断是否超过等待时间，让信号稳定在开启adc
        {
            learn.state = LEARN_START_ADC;
        }
        break;

    case LEARN_START_ADC:
        Start_ADC_Capture();
        learn.state = LEARN_WAIT_ADC;
        break;

    case LEARN_WAIT_ADC:
        if (ADC_Flag)
            learn.state = LEARN_PROCESS_FFT;
        break;

    case LEARN_PROCESS_FFT:
        FFT_SingleFreqResult_t input_dft;
        FFT_SingleFreqResult_t output_dft;
        FFT_TransferResult_t h;

        FFT_SingleFreqDFT_U16(ADC1_IN, ADC_LEN, 200000.0f, (float)learn.freq, &input_dft);
        FFT_SingleFreqDFT_U16(ADC2_OUT, ADC_LEN, 200000.0f, (float)learn.freq, &output_dft);

        FFT_CalcTransfer(&input_dft, &output_dft, &h);

        freq_table[learn.index] = (float)learn.freq;
        h_table[learn.index].r = h.real;
        h_table[learn.index].i = h.imag;
        learn.state = LEARN_NEXT_FREQ;
        break;

    case LEARN_NEXT_FREQ:
        if (learn.freq >= LEARN_STOP_FREQ_HZ)
        {
            learn.state = LEARN_CALC_IIR;
        }
        else
        {
            learn.freq += LEARN_STEP_FREQ_HZ;
            learn.index++;
            learn.state = LEARN_SET_FREQ;
        }
        break;

    case LEARN_CALC_IIR:
        coef_calc(h_table);
        analog_coef_data = matrix_calc();
        digital_coef_data = bilinear_transform_quant(&analog_coef_data);
        // 根据扫频得到的 H(jw) 计算 IIR 参数
        learn.state = LEARN_DONE;
        break;

    case LEARN_DONE:
        learn.running = 0;
        learn.state = LEARN_IDLE;
        break;

    default:
        learn.state = LEARN_IDLE;
        break;
    }
}
