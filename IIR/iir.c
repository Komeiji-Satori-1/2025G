#include "iir.h"
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
    lambda0_coef = 246.0; // 频率点数量
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
    for (uint8_t i = 0; i < SAMPLE_NUM; i++)
    {
        // 计算角频率 ω = 2π * 200Hz * (i+1) = 400π * (i+1) rad/s
        w1 = 2.0 * PI * freq_table[i]; // ω
        w2 = w1 * w1;                  // ω^2
        w3 = w2 * w1;                  // ω^3
        w4 = w3 * w1;                  // ω^4

        // 计算频率响应的模值平方: |H(jω)|^2 = Re^2 + Im^2
        modul_squre = sample_data[i].r * sample_data[i].r +
                      sample_data[i].i * sample_data[i].i;

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
        printf("Error: NULL pointer passed to bilinear_transform_quant\n");
        return result;
    }
    if (Fs <= 0 || Fs > 1e9)
    { // 补充Fs有效性检查（不修改端口，仅增强健壮性）
        printf("Error: Invalid sampling frequency Fs = %.2f (must be >0 and <=1e9)\n", Fs);
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
        printf("Error: Denominator coefficient den0 is zero!\n");
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
