#include "iir.h"

#define FIT_Q_STEPS        36U
#define FIT_Q_MIN          0.25
#define FIT_Q_MAX          50.0
#define FIT_MAG_FLOOR_RATE 0.05
#define FIT_MAX_FREQ_RATE  0.45
#define FIT_PASS_END_MIN_RATIO 0.65
#define FIT_STOP_END_MAX_RATIO 0.55

static float64_t fit_freq_data[SAMPLE_NUM];
static float64_t fit_mag_data[SAMPLE_NUM];
static uint16_t fit_point_count = 0U;

typedef struct
{
    FILTER_TYPE type;
    float64_t error;
    float64_t K;
    float64_t w0;
    float64_t Q;
} fit_result;

typedef struct
{
    float64_t low_ratio;
    float64_t high_ratio;
    float64_t trough_ratio;
} fit_shape_feature;

static fit_shape_feature calc_fit_shape_feature(void)
{
    fit_shape_feature shape = {0.0, 0.0, 0.0};
    float64_t peak_mag = 0.0;
    float64_t trough_mag = 1.0e300;
    float64_t low_sum = 0.0;
    float64_t high_sum = 0.0;
    uint16_t low_count;
    uint16_t high_count;
    uint16_t high_start;

    if (fit_point_count == 0U)
    {
        return shape;
    }

    low_count = fit_point_count / 20U;
    high_count = fit_point_count / 10U;

    if (low_count == 0U)
    {
        low_count = 1U;
    }

    if (high_count == 0U)
    {
        high_count = 1U;
    }

    high_start = fit_point_count - high_count;

    for (uint16_t i = 0U; i < fit_point_count; i++)
    {
        float64_t mag = fit_mag_data[i];

        if (mag > peak_mag)
        {
            peak_mag = mag;
        }

        if (mag < trough_mag)
        {
            trough_mag = mag;
        }

        if (i < low_count)
        {
            low_sum += mag;
        }

        if (i >= high_start)
        {
            high_sum += mag;
        }
    }

    if (peak_mag <= EPS)
    {
        return shape;
    }

    shape.low_ratio = (low_sum / (float64_t)low_count) / peak_mag;
    shape.high_ratio = (high_sum / (float64_t)high_count) / peak_mag;
    shape.trough_ratio = trough_mag / peak_mag;

    return shape;
}

static uint8_t is_fit_shape_allowed(FILTER_TYPE type, const fit_shape_feature *shape)
{
    uint8_t low_pass_end;
    uint8_t high_pass_end;
    uint8_t low_stop_end;
    uint8_t high_stop_end;

    if (shape == NULL)
    {
        return 0U;
    }

    low_pass_end = (shape->low_ratio >= FIT_PASS_END_MIN_RATIO) ? 1U : 0U;
    high_pass_end = (shape->high_ratio >= FIT_PASS_END_MIN_RATIO) ? 1U : 0U;
    low_stop_end = (shape->low_ratio <= FIT_STOP_END_MAX_RATIO) ? 1U : 0U;
    high_stop_end = (shape->high_ratio <= FIT_STOP_END_MAX_RATIO) ? 1U : 0U;

    switch (type)
    {
    case LOW_PASS_FILTER:
        return (uint8_t)(low_pass_end && high_stop_end);

    case HIGH_PASS_FILTER:
        return (uint8_t)(low_stop_end && high_pass_end);

    case BAND_PASS_FILTER:
        return (uint8_t)(low_stop_end && high_stop_end);

    case BAND_STOP_FILTER:
        return (uint8_t)(low_pass_end && high_pass_end &&
                         (shape->trough_ratio <= FIT_STOP_END_MAX_RATIO));

    default:
        return 0U;
    }
}

static float64_t get_model_base_mag(FILTER_TYPE type, float64_t w, float64_t w0, float64_t Q)
{
    float64_t x;
    float64_t x2;
    float64_t real_den;
    float64_t imag_den;
    float64_t den_mag;

    if ((w0 <= EPS) || (Q <= EPS))
    {
        return 0.0;
    }

    x = w / w0;
    x2 = x * x;
    real_den = 1.0 - x2;
    imag_den = x / Q;
    den_mag = sqrt(real_den * real_den + imag_den * imag_den);

    if (den_mag <= EPS)
    {
        return 0.0;
    }

    switch (type)
    {
    case LOW_PASS_FILTER:
        return 1.0 / den_mag;

    case HIGH_PASS_FILTER:
        return x2 / den_mag;

    case BAND_PASS_FILTER:
        return fabs(imag_den) / den_mag;

    case BAND_STOP_FILTER:
        return fabs(real_den) / den_mag;

    default:
        return 0.0;
    }
}

static fit_result fit_one_filter_type(FILTER_TYPE type, const float64_t *q_values, uint16_t q_count)
{
    fit_result best;
    float64_t max_mag = 0.0;
    float64_t mag_floor;

    best.type = type;
    best.error = 1.0e300;
    best.K = 0.0;
    best.w0 = 0.0;
    best.Q = 0.0;

    for (uint16_t i = 0U; i < fit_point_count; i++)
    {
        if (fit_mag_data[i] > max_mag)
        {
            max_mag = fit_mag_data[i];
        }
    }

    if (max_mag <= EPS)
    {
        return best;
    }

    mag_floor = max_mag * FIT_MAG_FLOOR_RATE;
    if (mag_floor < EPS)
    {
        mag_floor = EPS;
    }

    for (uint16_t f0_index = 0U; f0_index < fit_point_count; f0_index++)
    {
        float64_t w0 = 2.0 * PI * fit_freq_data[f0_index];

        if (w0 <= EPS)
        {
            continue;
        }

        for (uint16_t q_index = 0U; q_index < q_count; q_index++)
        {
            float64_t Q = q_values[q_index];
            float64_t sum_ym = 0.0;
            float64_t sum_mm = 0.0;
            float64_t sum_yy = 0.0;
            float64_t K;
            float64_t error;

            for (uint16_t i = 0U; i < fit_point_count; i++)
            {
                float64_t y = fit_mag_data[i];
                float64_t w = 2.0 * PI * fit_freq_data[i];
                float64_t m = get_model_base_mag(type, w, w0, Q);
                float64_t weight_base = y;
                float64_t weight;

                if (weight_base < mag_floor)
                {
                    weight_base = mag_floor;
                }

                weight = 1.0 / (weight_base * weight_base);
                sum_ym += weight * y * m;
                sum_mm += weight * m * m;
                sum_yy += weight * y * y;
            }

            if (sum_mm <= EPS)
            {
                continue;
            }

            K = sum_ym / sum_mm;
            if (K <= 0.0)
            {
                continue;
            }

            error = sum_yy - 2.0 * K * sum_ym + K * K * sum_mm;
            if (error < best.error)
            {
                best.error = error;
                best.K = K;
                best.w0 = w0;
                best.Q = Q;
            }
        }
    }

    return best;
}

static analog_coef build_standard_analog_coef(const fit_result *fit)
{
    analog_coef coef = {0.0, 0.0, 0.0, 0.0, 0.0};
    float64_t w0;
    float64_t w02;
    float64_t Q;
    float64_t K;

    if (fit == NULL)
    {
        return coef;
    }

    w0 = fit->w0;
    w02 = w0 * w0;
    Q = fit->Q;
    K = fit->K;

    if ((w0 <= EPS) || (w02 <= EPS) || (Q <= EPS))
    {
        return coef;
    }

    coef.a1 = 1.0 / (Q * w0);
    coef.a2 = 1.0 / w02;

    switch (fit->type)
    {
    case LOW_PASS_FILTER:
        coef.b0 = K;
        break;

    case HIGH_PASS_FILTER:
        coef.b2 = K / w02;
        break;

    case BAND_PASS_FILTER:
        coef.b1 = K / (Q * w0);
        break;

    case BAND_STOP_FILTER:
        coef.b0 = K;
        coef.b2 = K / w02;
        break;

    default:
        break;
    }

    return coef;
}
/******************************************************************************
 * 全局变量 - 矩阵方程系数
 *
 * 用于构建最小二乘拟合的线性方程组: M * N = C
 * 其中 N = [N0, N1, N2, N3, N4]^T 是待求解的模拟滤波器系数向量
 *
 * 方程组的构建基于频域响应数据 result_array[]，通过最小化误差来拟合
 * 二阶模拟滤波器的分子和分母系数。
 ******************************************************************************/

// Lambda 系数 - 用于矩阵 M 的频率相关项
float64_t lambda0_coef; // λ0 = 常数项 (频率响应点的数量)
float64_t lambda2_coef; // λ2 = Σ(ω^2) - 二阶频率项之和
float64_t lambda4_coef; // λ4 = Σ(ω^4) - 四阶频率项之和

// S 系数 - 频率响应实部相关项
float64_t s0_coef; // s0 = Σ(Re[H(jω)]) - 实部之和
float64_t s2_coef; // s2 = Σ(ω^2 * Re[H(jω)]) - 加权实部之和
float64_t s4_coef; // s4 = Σ(ω^4 * Re[H(jω)]) - 高阶加权实部之和

// T 系数 - 频率响应虚部相关项
float64_t t1_coef; // t1 = Σ(ω * Im[H(jω)]) - 加权虚部之和
float64_t t3_coef; // t3 = Σ(ω^3 * Im[H(jω)]) - 高阶加权虚部之和

// U 系数 - 频率响应幅度相关项
float64_t u2_coef; // u2 = Σ(ω^2 * |H(jω)|^2) - 加权幅度平方之和
float64_t u4_coef; // u4 = Σ(ω^4 * |H(jω)|^2) - 高阶加权幅度平方之和

/******************************************************************************
 * @brief   计算矩阵方程的系数
 * @details 遍历所有频域响应数据点，计算用于构建线性方程组的系数
 *
 * 功能说明：
 * 该函数计算最小二乘拟合所需的所有累加项，这些累加项将用于构建
 * 5x5 矩阵方程 M*N = C，其中 N 是待求解的模拟滤波器系数向量
 *
 * 计算的系数包括：
 * - Lambda 系数: 频率的幂次项 (ω^2, ω^4)
 * - S 系数: 频率响应实部的加权和
 * - T 系数: 频率响应虚部的加权和
 * - U 系数: 频率响应模值平方的加权和
 *
 * 频率点计算：
 * ω = 2π * 200Hz * (i+1) = 400π * (i+1) rad/s
 * 对应频率: 200Hz, 400Hz, 600Hz, ..., 50kHz (共250个点)
 *
 * @note 必须在调用 matrix_calc() 之前调用此函数
 * @note 函数会自动初始化所有全局系数变量为0
 ******************************************************************************/
void coef_calc(const complex *sample_data)
{
    float64_t w1, w2, w3, w4; // ω的1,2,3,4次方
    float64_t modul_squre;    // 频率响应的模值平方 |H(jω)|^2

    // 初始化所有系数为0 (lambda0除外)
    lambda0_coef = 500.0; // 频率点数量
    fit_point_count = 0U;
    lambda2_coef = 0.0;
    lambda4_coef = 0.0;
    s0_coef = 0.0;
    s2_coef = 0.0;
    s4_coef = 0.0;
    t1_coef = 0.0;
    t3_coef = 0.0;
    u2_coef = 0.0;
    u4_coef = 0.0;

    // 遍历所有频率点，累加计算各项系数
    for (uint16_t i = 0; i < SAMPLE_NUM; i++)
    {
        // 计算角频率 ω = 2π * 200Hz * (i+1) = 400π * (i+1) rad/s
        w1 = 2.0 * PI * freq_table[i]; // ω
        w2 = w1 * w1;                  // ω^2
        w3 = w2 * w1;                  // ω^3
        w4 = w3 * w1;                  // ω^4

        // 计算频率响应的模值平方: |H(jω)|^2 = Re^2 + Im^2
        modul_squre = sample_data[i].r * sample_data[i].r +
                      sample_data[i].i * sample_data[i].i;

        if ((freq_table[i] > 0.0f) && (freq_table[i] <= (float)(Fs * FIT_MAX_FREQ_RATE)) && (modul_squre > (EPS * EPS)))
        {
            fit_freq_data[fit_point_count] = (float64_t)freq_table[i];
            fit_mag_data[fit_point_count] = sqrt(modul_squre);
            fit_point_count++;
        }

        // 累加 Lambda 系数 (频率项)
        lambda2_coef += w2; // Σ(ω^2)
        lambda4_coef += w4; // Σ(ω^4)

        // 累加 S 系数 (实部相关项)
        s0_coef += sample_data[i].r;      // Σ(Re[H])
        s2_coef += w2 * sample_data[i].r; // Σ(ω^2 * Re[H])
        s4_coef += w4 * sample_data[i].r; // Σ(ω^4 * Re[H])

        // 累加 T 系数 (虚部相关项)
        t1_coef += w1 * sample_data[i].i; // Σ(ω * Im[H])
        t3_coef += w3 * sample_data[i].i; // Σ(ω^3 * Im[H])

        // 累加 U 系数 (模值平方相关项)
        u2_coef += w2 * modul_squre; // Σ(ω^2 * |H|^2)
        u4_coef += w4 * modul_squre; // Σ(ω^4 * |H|^2)
    }
}

/******************************************************************************
 * @brief   求解矩阵方程，得到模拟滤波器系数
 * @return  analog_coef 结构体，包含模拟滤波器的分子和分母系数
 * @details 通过求解线性方程组 M*N = C 来获得模拟滤波器的传递函数系数
 *
 * 理论基础：
 * 目标是拟合一个二阶模拟滤波器: H(s) = (b2*s^2 + b1*s + b0) / (a2*s^2 + a1*s + 1)
 * 通过最小二乘法最小化频域误差: Σ|H_measured(jω) - H_model(jω)|^2
 *
 * 线性方程组形式: M * N = C
 * 其中:
 * - M: 5x5 系数矩阵 (由 coef_calc() 计算的系数构成)
 * - N: 5x1 未知向量 [N0, N1, N2, N3, N4]^T
 * - C: 5x1 常数向量
 *
 * 系数矩阵 M 的结构:
 *     ┌                                                           ┐
 *     │  λ0      0     -λ2      t1      s2                      │
 *     │   0     λ2       0     -s2      t3                       │
 *     │  λ2      0     -λ4      t3      s4                      │
 *     │  t1    -s2     -t3      u2       0                        │
 *     │  s2     t3     -s4       0      u4                        │
 *     └                                                           ┘
 *
 * N 向量与模拟滤波器系数的关系:
 * - N0 = b0 (分子常数项)
 * - N1 = b1 (分子一次项系数)
 * - N2 = b2 (分子二次项系数)
 * - N3 = a1 (分母一次项系数)
 * - N4 = a2 (分母二次项系数)
 *
 * 求解方法:
 * 1. 计算矩阵 M 的逆: M_inv = M^(-1)
 * 2. 求解: N = M_inv * C
 *
 * @note 调用前必须先执行 coef_calc() 计算所有系数
 * @note 使用 ARM CMSIS-DSP 库进行矩阵运算
 ******************************************************************************/
analog_coef matrix_calc(void)
{
#if 1
    analog_coef analog_coef_temp = {0.0, 0.0, 0.0, 0.0, 0.0};
    float64_t q_values[FIT_Q_STEPS];
    fit_result results[4];
    fit_result best;
    fit_shape_feature shape;
    uint8_t shape_ok[4] = {0U, 0U, 0U, 0U};
    uint8_t has_shape_match = 0U;
    float64_t q_ratio;

    if (fit_point_count < 5U)
    {
        printf("Error: Not enough points for magnitude-only IIR fit!\n");
        return analog_coef_temp;
    }

    q_ratio = pow(FIT_Q_MAX / FIT_Q_MIN, 1.0 / (float64_t)(FIT_Q_STEPS - 1U));
    q_values[0] = FIT_Q_MIN;
    for (uint16_t i = 1U; i < FIT_Q_STEPS; i++)
    {
        q_values[i] = q_values[i - 1U] * q_ratio;
    }

    results[0] = fit_one_filter_type(LOW_PASS_FILTER, q_values, FIT_Q_STEPS);
    results[1] = fit_one_filter_type(HIGH_PASS_FILTER, q_values, FIT_Q_STEPS);
    results[2] = fit_one_filter_type(BAND_PASS_FILTER, q_values, FIT_Q_STEPS);
    results[3] = fit_one_filter_type(BAND_STOP_FILTER, q_values, FIT_Q_STEPS);

    shape = calc_fit_shape_feature();
    for (uint8_t i = 0U; i < 4U; i++)
    {
        shape_ok[i] = is_fit_shape_allowed(results[i].type, &shape);
        if (shape_ok[i] != 0U)
        {
            if ((has_shape_match == 0U) || (results[i].error < best.error))
            {
                best = results[i];
            }
            has_shape_match = 1U;
        }
    }

    if (has_shape_match == 0U)
    {
        best = results[0];
        for (uint8_t i = 1U; i < 4U; i++)
        {
            if (results[i].error < best.error)
            {
                best = results[i];
            }
        }
    }

    analog_coef_temp = build_standard_analog_coef(&best);

    printf("\nMagnitude-only IIR fit:\n");
    printf("Used points = %u\n", fit_point_count);
    printf("Endpoint shape: low/peak = %.6f, high/peak = %.6f, trough/peak = %.6f\n",
           shape.low_ratio,
           shape.high_ratio,
           shape.trough_ratio);
    printf("LP error = %.12f, shape_ok = %u\n", results[0].error, shape_ok[0]);
    printf("HP error = %.12f, shape_ok = %u\n", results[1].error, shape_ok[1]);
    printf("BP error = %.12f, shape_ok = %u\n", results[2].error, shape_ok[2]);
    printf("BS error = %.12f, shape_ok = %u\n", results[3].error, shape_ok[3]);
    if (has_shape_match == 0U)
    {
        printf("Warning: no filter type passed endpoint shape rules, using minimum error only.\n");
    }
    printf("Best type = %d, K = %.12f, f0 = %.6f Hz, Q = %.12f\n",
           best.type,
           best.K,
           best.w0 / (2.0 * PI),
           best.Q);

    printf("Solution N = [N0, N1, N2, N3, N4]^T:\n");
    printf("N0 = %.20f\n", analog_coef_temp.b0);
    printf("N1 = %.20f\n", analog_coef_temp.b1);
    printf("N2 = %.20f\n", analog_coef_temp.b2);
    printf("N3 = %.20f\n", analog_coef_temp.a1);
    printf("N4 = %.20f\n", analog_coef_temp.a2);

    return analog_coef_temp;
#else

    // ==================== 1. 构建系数矩阵 M (5x5) ====================
    // 矩阵按行优先存储
    float64_t M_data[MATRIX_SIZE * MATRIX_SIZE] = {
        // 第1行: [λ0,   0,    -λ2,   t1,   s2]
        lambda0_coef, 0.0, -lambda2_coef, t1_coef, s2_coef,
        // 第2行: [0,    λ2,    0,   -s2,   t3]
        0.0, lambda2_coef, 0.0, -s2_coef, t3_coef,
        // 第3行: [λ2,   0,   -λ4,    t3,   s4]
        lambda2_coef, 0.0, -lambda4_coef, t3_coef, s4_coef,
        // 第4行: [t1,  -s2,   -t3,    u2,    0]
        t1_coef, -s2_coef, -t3_coef, u2_coef, 0.0,
        // 第5行: [s2,   t3,   -s4,     0,   u4]
        s2_coef, t3_coef, -s4_coef, 0.0, u4_coef};

    // ==================== 2. 构建常数向量 C (5x1) ====================
    float64_t C_data[VECTOR_SIZE] = {s0_coef, t1_coef, s2_coef, 0.0, u2_coef};

    // ==================== 3. 定义矩阵结构体 ====================
    analog_coef analog_coef_temp;  // 返回结果
    arm_matrix_instance_f64 M;     // 系数矩阵 M
    arm_matrix_instance_f64 M_inv; // M 的逆矩阵
//    arm_matrix_instance_f64 C;     // 常数向量 C
//    arm_matrix_instance_f64 N;     // 解向量 N

    // ==================== 4. 分配缓冲区 ====================
    float64_t M_inv_data[MATRIX_SIZE * MATRIX_SIZE]; // 逆矩阵缓冲区 (5x5=25元素)
    float64_t N_data[VECTOR_SIZE];                   // 解向量缓冲区 (5元素)

    // ==================== 5. 初始化矩阵结构体 ====================
    M.numRows = MATRIX_SIZE;
    M.numCols = MATRIX_SIZE;
    M.pData = M_data;

    M_inv.numRows = MATRIX_SIZE;
    M_inv.numCols = MATRIX_SIZE;
    M_inv.pData = M_inv_data;
   
    arm_status status;
//    arm_mat_init_f64(&M, MATRIX_SIZE, MATRIX_SIZE, M_data);         // M: 5x5
//    arm_mat_init_f64(&M_inv, MATRIX_SIZE, MATRIX_SIZE, M_inv_data); // M_inv: 5x5
//    arm_mat_init_f64(&C, MATRIX_SIZE, 1, C_data);                   // C: 5x1
//    arm_mat_init_f64(&N, MATRIX_SIZE, 1, N_data);                   // N: 5x1

    // ==================== 6. 求解线性方程组 ====================

    // 步骤1: 计算 M 的逆矩阵 M_inv = M^(-1)
    status = arm_mat_inverse_f64(&M, &M_inv);
    if (status != ARM_MATH_SUCCESS)
    {
        // 矩阵奇异(行列式为0)，无法求逆
        printf("Error: Matrix M is singular (cannot be inverted)!\n");
    }

    // 步骤2: 矩阵乘法 N = M_inv * C (5x5 矩阵乘 5x1 向量 = 5x1 向量)
    for (uint8_t row = 0; row < MATRIX_SIZE; row++)
    {
        N_data[row] = 0.0;

        for (uint8_t col = 0; col < MATRIX_SIZE; col++)
        {
            N_data[row] += M_inv_data[row * MATRIX_SIZE + col] * C_data[col];
        }
    }

    if (status != ARM_MATH_SUCCESS)
    {
        // 矩阵乘法失败(维度不匹配等)
        printf("Error: Matrix multiplication failed!\n");
    }

    // ==================== 7. 输出求解结果(可选调试) ====================
    printf("Solution N = [N0, N1, N2, N3, N4]^T:\n");
    for (int i = 0; i < VECTOR_SIZE; i++)
    {
        printf("N%d = %.20f\n", i, N_data[i]);
    }

    // ==================== 8. 提取模拟滤波器系数 ====================
    // 将解向量 N 映射到模拟滤波器系数结构体
    analog_coef_temp.b0 = N_data[0]; // 分子常数项
    analog_coef_temp.b1 = N_data[1]; // 分子一次项系数
    analog_coef_temp.b2 = N_data[2]; // 分子二次项系数

    analog_coef_temp.a1 = N_data[3]; // 分母一次项系数
    analog_coef_temp.a2 = N_data[4]; // 分母二次项系数

    return analog_coef_temp;
#endif
}

/******************************************************************************
 * @brief   复数除法运算
 * @param   data0 被除数(分子复数) 指针
 * @param   data1 除数(分母复数) 指针
 * @return  商 data0/data1 的复数结果
 * @details 实现复数除法: (a + jb) / (c + jd)
 *
 * 计算公式:
 * 设 z1 = a + jb, z2 = c + jd
 * z1/z2 = (z1 * z2*) / |z2|^2
 *       = [(a + jb)(c - jd)] / (c^2 + d^2)
 *       = [(ac + bd) + j(bc - ad)] / (c^2 + d^2)
 *
 * 实部: Re = (ac + bd) / (c^2 + d^2)
 * 虚部: Im = (bc - ad) / (c^2 + d^2)
 *
 * @note 不检查除数是否为0，调用者需确保 data1 不为0
 ******************************************************************************/
complex complex_div(const complex *data0, const complex *data1)
{
    complex temp;
    float64_t modul_squre;

    // 计算分母的模值平方: |z2|^2 = c^2 + d^2
    modul_squre = data1->r * data1->r + data1->i * data1->i;

    // 计算商的实部: (ac + bd) / |z2|^2
    temp.r = (data0->r * data1->r + data0->i * data1->i) / modul_squre;

    // 计算商的虚部: (bc - ad) / |z2|^2
    temp.i = (data1->r * data0->i - data0->r * data1->i) / modul_squre;

    return temp;
}

/******************************************************************************
 * @brief   双线性变换 + Q31量化：将模拟滤波器转换为定点数字滤波器
 * @param   analog_coef_data 输入的模拟滤波器系数结构体指针
 * @return  digital_coef 包含Q31格式量化后的数字滤波器系数
 * @details 使用双线性变换将连续时间(s域)滤波器转换为离散时间(z域)滤波器
 *
 * ============================================================================
 * 理论基础 - 双线性变换 (Bilinear Transform)
 * ============================================================================
 *
 * 1. 变换公式:
 *    s = K * (1 - z^-1) / (1 + z^-1), 其中 K = 2*Fs
 *
 * 2. 输入模拟滤波器:
 *    H(s) = (b2*s^2 + b1*s + b0) / (a2*s^2 + a1*s + 1)
 *
 * 3. 输出数字滤波器:
 *    H(z) = (b0 + b1*z^-1 + b2*z^-2) / (1 + a1*z^-1 + a2*z^-2)
 *
 * 4. 转换步骤:
 *    - 将 s 替换为 K*(1-z^-1)/(1+z^-1)
 *    - 展开并整理为 z^-1 的多项式形式
 *    - 归一化使分母首项系数为 1
 *
 * 5. 系数计算 (详见代码注释):
 *    分子: num0, num1, num2 (对应 z^0, z^-1, z^-2)
 *    分母: den0, den1, den2 (对应 z^0, z^-1, z^-2, 归一化后 den0=1)
 *
 * ============================================================================
 * 量化说明 - Q31 定点格式
 * ============================================================================
 *
 * Q31 格式: 32位有符号整数，隐含小数点在最高位之后
 * 表示范围: [-1, +1)
 * 精度: 2^-31 ≈ 4.66e-10
 *
 * 转换公式: int32_value = round(float_value * 2^31)
 *
 * 例如:
 *  0.5      → 1073741824  (2^30)
 * -0.5      → -1073741824
 *  0.999... → 2147483647  (2^31-1)
 * -1.0      → -2147483648 (-2^31)
 *
 * ============================================================================
 * 重要说明
 * ============================================================================
 *
 * 1. 无预畸变: 本函数假设输入的模拟系数已经是实际频率下的系数，
 *    不进行频率预畸变 (pre-warping)。
 *
 * 2. 采样频率: 使用头文件定义的 Fs (默认 2MHz)
 *
 * 3. 溢出保护: 如果浮点系数超过 [-1, 1] 范围，会自动缩放
 *
 * 4. 返回值: 结构体包含 5 个Q31格式系数: b0, b1, b2, a1, a2
 *    (注意: 分母归一化后 a0 = 1，不需要返回)
 *
 * @note 调用前应先调用 matrix_calc() 获取模拟滤波器系数
 ******************************************************************************/
digital_coef bilinear_transform_quant(const analog_coef *analog_coef_data)
{
    // 返回值初始化（默认全0）
    digital_coef result = {0, 0, 0, 0, 0};

    // ========== 步骤0: 输入参数检查（保持原逻辑，补充Fs校验） ==========
    if (analog_coef_data == NULL)
    {
        fprintf(stderr, "Error: NULL pointer passed to bilinear_transform_quant\n");
        return result;
    }
    if (Fs <= 0 || Fs > 1e9)
    { // 补充Fs有效性检查（不修改端口，仅增强健壮性）
        fprintf(stderr, "Error: Invalid sampling frequency Fs = %.2f (must be >0 and <=1e9)\n", Fs);
        return result;
    }

    // ========== 步骤1: 提取模拟系数（保持原逻辑，分母默认A0=1） ==========
    double B0 = analog_coef_data->b0;
    double B1 = analog_coef_data->b1;
    double B2 = analog_coef_data->b2;
    double A1 = analog_coef_data->a1;
    double A2 = analog_coef_data->a2;

    printf("\nAnalog filter coefficients:\n");
    printf("B = [%.12f, %.12f, %.12f]\n", B2, B1, B0);
    printf("A = [%.12f, %.12f, 1.000000000000]\n", A2, A1);

    // ========== 步骤2: 双线性变换（核心错误修复） ==========
    double K = 2.0 * Fs;
    double K2 = K * K;

    // 分子系数计算（原逻辑正确，保留）
    double num0 = B2 * K2 + B1 * K + B0;     // z⁰项
    double num1 = -2.0 * B2 * K2 + 2.0 * B0; // z⁻¹项
    double num2 = B2 * K2 - B1 * K + B0;     // z⁻²项

    // 分母系数计算（修复：den0 补充 A1*K 项）
    double den0 = A2 * K2 + A1 * K + 1.0; // 原错误：漏掉 A1*K
    double den1 = -2.0 * A2 * K2 + 2.0;   // 原逻辑正确
    double den2 = A2 * K2 - A1 * K + 1.0; // 原逻辑正确

    // 检查分母常数项非零（避免数值爆炸）
    if (fabs(den0) < EPS)
    {
        fprintf(stderr, "Error: Denominator coefficient den0 is zero!\n");
        return result;
    }

    // 分母归一化（保持原逻辑：让分母常数项为1）
    num0 /= den0;
    num1 /= den0;
    num2 /= den0;
    den1 /= den0;
    den2 /= den0;

    printf("\nDigital filter coefficients (float):\n");
    printf("num = [%.12f, %.12f, %.12f]\n", num0, num1, num2);
    printf("den = [1.000000000000, %.12f, %.12f]\n", den1, den2);

    // ========== 步骤4: Q31量化（修复：补充a2量化，避免溢出） ==========
    result.b0 = (int64_t)round(num0 * Q48_SCALE);
    result.b1 = (int64_t)round(num1 * Q48_SCALE);
    result.b2 = (int64_t)round(num2 * Q48_SCALE);
    result.a1 = (int64_t)round(den1 * Q48_SCALE);
    result.a2 = (int64_t)round(den2 * Q48_SCALE);

    // ========== 步骤5: 打印最终结果（修复a2打印） ==========
    printf("\nFinal quantized coefficients (32-bit Q31):\n");
    printf("b0 = %lld, b1 = %lld, b2 = %lld\n", result.b0, result.b1, result.b2);
    printf("a1 = %lld, a2 = %lld\n", result.a1, result.a2);

    return result;
}

/******************************************************************************
 * @brief   滤波器类型判断函数
 * @param   coef:模拟滤波器系数指针
 * @return  滤波器类型
 * @details 通过判断|H(0)|与|H(inf)|判断滤波器类型
 *
 ******************************************************************************/

FILTER_TYPE get_filter_type(const analog_coef * coef) {
  
    if (coef == NULL) {
        return LOW_PASS_FILTER; 
    }
    
    //|H(0)|计算 H(0) = |b0|
    float64_t H_0 = fabs(coef->b0);
    
    //|H(inf)|计算 H(inf) = |b2/a2|
    float64_t H_inf = fabs(coef->b2 / coef->a2);
    
    printf("\nFilter Type Analysis:\n");
    printf("|H(0)| = %.6f\n", H_0);
    printf("|H(inf)| = %.6f\n", H_inf);
    
    //判断滤波器类型
    if (H_0 > FILTER_MODE_THR && H_inf < FILTER_MODE_THR)
    {
        HMI_send_string("t5", "LOW_PASS_FILTER");
        printf("Filter Type: LOW_PASS_FILTER\n");
        return LOW_PASS_FILTER;
    }else if (H_0 < FILTER_MODE_THR && H_inf > FILTER_MODE_THR)
    {
        HMI_send_string("t5", "HIGH_PASS_FILTER");
        printf("Filter Type: HIGH_PASS_FILTER\n");
        return HIGH_PASS_FILTER;
    }
    else if (H_0 < FILTER_MODE_THR && H_inf < FILTER_MODE_THR)
    {
        HMI_send_string("t5", "BAND_PASS_FILTER");
        printf("Filter Type: BAND_PASS_FILTER\n");
        return BAND_PASS_FILTER;
    }else if (H_0 > FILTER_MODE_THR && H_inf > FILTER_MODE_THR)
    {
        HMI_send_string("t5", "BAND_STOP_FILTER");
        printf("Filter Type: BAND_STOP_FILTER\n");
        return BAND_STOP_FILTER;
    }else{
        HMI_send_string("t5", "UNKNOWN_FILTER");
        printf("Filter Type: UNKNOWN - Defaulting to LOW_PASS_FILTER\n");
        return LOW_PASS_FILTER;
    }
}
