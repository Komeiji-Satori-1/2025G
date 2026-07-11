# ADC-DAC 同步实时 IIR 方案

日期: 2026-07-11

## 目标边界

本方案面向 STM32H743IIT6，先只描述 IOC 配置层和代码实现层，不修改当前 `.ioc` 或源码。目标是在图 4 对比测试中，让探究装置根据同一个信号发生器输入，实时生成与未知无源 RLC 模型输出峰峰值一致的波形。当前阶段不展开 FPGA 实现。

确认约束如下:

- ADC 输入: PA0 / ADC1_INP16，单通道实时采样。
- DAC 输出: PA4 / DAC Channel 1，单通道输出到示波器。
- 同步时钟: TIM3 TRGO。
- 采样率: 200 kHz。
- 缓冲区: 总长度 1024 点，半缓冲 512 点。
- IIR 阶数: 只使用已有二阶 IIR 方案。
- 验证方式: 示波器观察 DAC 输出波形和未知模型输出波形的峰峰值。
- 信号特性: 输入为以 0V 为中心摆动的未知周期信号，峰峰值不超过 2V，可能出现负电压。

## 硬件接口方案

题目要求探究装置输入电阻不小于 100 kOhm。信号源输出需要同时进入未知模型电路和探究装置，因此探究装置输入前端不能明显加载信号源。

推荐结构见 [adc_dac_sync_iir_frontend.svg](img/adc_dac_sync_iir_frontend.svg)。

### ADC 输入前端

信号源输出一路直接进入未知模型电路，另一路进入探究装置 ADC 前端。由于 STM32H743 ADC 不能采负电压，探究装置内部采用 1.65V 直流偏置工作。

推荐链路:

```text
信号源 -> 高输入阻抗保护/缓冲 -> 叠加 1.65V 偏置 -> ADC1 PA0
```

实现要求:

- 输入端等效阻抗应不小于 100 kOhm。
- 输入峰峰值不超过 2V 时，偏置后 ADC 电压约为 0.65V 到 2.65V。
- 1.65V 偏置由已完成的电阻分压模块提供，建议经过运放缓冲后再参与叠加或作为测量基准。
- ADC 前应保留限流电阻和钳位/保护空间，防止误接或瞬态超过 ADC 允许范围。
- H743 ADC 采样电容对源阻抗敏感，前端输出到 ADC 的节点建议由运放缓冲，避免高阻直接驱动 ADC。

OPA2228、TL082 等正负电源轨运放可用于高阻缓冲和偏置叠加。因为已有正负电源轨，前端可以先在双电源域处理零中心信号，再把输出平移到 MCU ADC 可采范围。

### DAC 输出后级

PA4 / DAC Channel 1 只能输出 0V 到 3.3V。探究装置内部建议输出带 1.65V 偏置的 DAC 波形:

```text
Vdac = y_ac + 1.65V
```

示波器比较时推荐两个通道都使用 AC 耦合，直接比较峰峰值和波形稳定性。若现场要求 DC 耦合且输出必须以 0V 为中心，则在 DAC 后增加运放减法或隔直输出级，把 1.65V 偏置去掉后再接示波器。

## IOC 配置层参数表

当前 `.ioc` 已经存在 PA0/ADC1_INP16 和 TIM3 TRGO 触发，但 DMA 仍是 normal/oneshot，且未看到 DAC 外设配置。下一步进入实现时建议按下表调整。

| 模块 | 参数 | 建议值 | 说明 |
|---|---|---|---|
| ADC1 | Channel | ADC1_INP16 / PA0 | 探究装置信号输入 |
| ADC1 | Resolution | 16-bit | 保持当前高分辨率 |
| ADC1 | External Trigger | TIM3 TRGO rising edge | 与 DAC 共用时基 |
| ADC1 | ConversionDataManagement | DMA circular | 当前是 DMA oneshot，需要改为循环实时流 |
| ADC1 DMA | Direction | Peripheral to Memory | ADC 数据进入输入缓冲区 |
| ADC1 DMA | Mode | Circular | 支持半缓冲和全缓冲持续处理 |
| ADC1 DMA | Data width | Halfword | 匹配 16-bit ADC 样本 |
| ADC1 DMA | Interrupt | Half Transfer + Transfer Complete | 512 点块处理 |
| DAC1 | Channel | DAC Channel 1 / PA4 | 探究装置输出 |
| DAC1 | Trigger | TIM3 TRGO | 与 ADC 同步更新 |
| DAC1 DMA | Direction | Memory to Peripheral | 输出缓冲区自动送 DAC |
| DAC1 DMA | Mode | Circular | 保持连续波形输出 |
| DAC1 DMA | Data width | Halfword | 12-bit DAC 数据放入 halfword |
| TIM3 | Master Output Trigger | Update event TRGO | 同步触发 ADC 和 DAC |
| TIM3 | Update rate | 200 kHz | 与 `IIR/iir.h` 中 `Fs=200000.0` 对齐 |

TIM3 当前生成代码中计数时钟标注为 240 MHz，现有配置为 Prescaler = 12-1、Period = 100-1，对应更新频率约 200 kHz。后续若时钟树变动，需要重新核算该频率。

## 代码实现层清单

当前代码库中已有如下基础:

- `Core/Inc/main.h` 中 `ADC_LEN` 已为 1024。
- `Core/Src/main.c` 中已有 `ADC1_IN[ADC_LEN]` 和 `ADC2_OUT[ADC_LEN]` 缓冲区。
- `IIR/iir.h` 中 `Fs` 已定义为 200000.0。
- `IIR/iir.c` 已包含二阶模型拟合、滤波类型判断和双线性变换量化相关代码。

下一步代码实现建议按模块边界处理:

| 模块 | 职责 | 注意事项 |
|---|---|---|
| 实时输入缓冲 | 保存 ADC DMA 采样值，长度 1024 | 放在 DMA 可访问内存区，或做 DCache 维护 |
| 实时输出缓冲 | 保存 DAC DMA 输出值，长度 1024 | DAC 启动前先填入 1.65V 中点码 |
| 半缓冲调度 | 在 half/full 回调中选择 0..511 或 512..1023 | 中断内只做确定耗时操作，避免阻塞 |
| 去偏置转换 | ADC 码值 -> 电压 -> 减实际偏置 | 不固定假设 1.65V 精确等于实际偏置 |
| 二阶 IIR 执行 | 对去偏置后的样本逐点执行差分方程 | 复用已有二阶系数，不改现有函数名和变量名 |
| DAC 码值生成 | IIR 输出 + 1.65V -> DAC 码值 | 需要限幅到 0..4095 |
| 测时/验证 | 可选 GPIO 翻转测半缓冲处理时间 | 判定条件是处理时间小于 512/200k = 2.56 ms |

推荐实时流程:

```text
TIM3 update -> ADC1 采样，同时 DAC1 更新
ADC DMA 写入输入缓冲区
Half Transfer: 处理前 512 点，写入 DAC 输出缓冲区前半段
Transfer Complete: 处理后 512 点，写入 DAC 输出缓冲区后半段
DAC DMA 循环输出已处理数据
```

## 去直流偏置策略

算法内部应处理交流量，而不是直接处理带 1.65V 偏置的 ADC 原始值。

基本公式:

```text
x_ac = adc_voltage - v_bias_actual
y_dac_voltage = y_ac + v_bias_output
```

`v_bias_actual` 不建议写死为 1.65V。建议优先做启动校准: 在输入短接到 0V 或确认无交流输入时采集一段 ADC 平均值，将该平均值作为实际偏置。运行中若需要漂移补偿，只允许使用很慢的均值跟踪，避免把低频信号或非 50% 占空比矩形波的真实平均值错误扣掉。

示波器比较时，两路都可用 AC 耦合。这样未知模型输出是零中心无源滤波结果，探究装置输出虽然物理上带 1.65V 偏置，但示波器显示可直接比较峰峰值。

## DCache 与内存风险

STM32H743 开启 DCache 后，DMA 缓冲区可能出现 CPU 看到的数据和 DMA 实际写入数据不一致的问题。后续实现必须二选一:

- 将 ADC/DAC DMA 缓冲区放入 DMA 可访问且非缓存区域；或
- 在 half/full 回调中对 ADC 输入半缓冲做 invalidate，对 DAC 输出半缓冲做 clean。

若不处理 DCache，表现可能是波形偶发错误、旧数据重复输出、调试时内存值和示波器不一致。

## 验证标准

第一阶段只验证实时链路跑通:

- 输入 1 kHz、2 Vpp 正弦波，示波器 AC 耦合观察 PA4 输出，波形稳定连续。
- 输入 1 kHz、2 Vpp 矩形波，DAC 输出无明显断点或周期性卡顿。
- 半缓冲处理时间小于 2.56 ms。
- DAC 输出峰峰值随 IIR 系数变化符合预期。

第二阶段再和未知模型电路对比:

- 信号源同时接未知模型和探究装置。
- 示波器 1 看未知模型输出，示波器 2 看 PA4 或 DAC 后级输出。
- 两路都使用 AC 耦合时，峰峰值相对误差满足题目要求。

## 待确认风险

- DAC 输出若直接用 PA4，实际只能输出带偏置波形；若裁判要求 DC 耦合零中心输出，需要追加 DAC 后级去偏置硬件。
- ADC 前端偏置叠加电路必须确认不会把信号源负载拉低到 100 kOhm 以下。
- 现有代码中还有 ADC2 采样路径，后续实时方案若只用 ADC1，需要决定 ADC2 是保留给建模采集，还是在实时输出阶段停用。
- 后续 IOC 生成代码会覆盖部分 `Core/Src/*.c`，改动前必须再次检查分支、工作区和远程状态。
