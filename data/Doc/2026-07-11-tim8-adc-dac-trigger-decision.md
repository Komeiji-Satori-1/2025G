# TIM8 统一触发 ADC/DAC 的决策记录

日期: 2026-07-11

## 背景

原实时采样与输出方案计划使用 TIM3 作为统一触发源，同时触发 ADC 采样和 DAC 更新。但经 IOC/CubeMX 配置检查，DAC1 Channel 1 / PA4 无法按预期选择 TIM3 作为触发源。为保持 ADC 与 DAC 仍由同一个定时器同步驱动，统一触发源调整为 TIM8 TRGO。

## 当前硬件与外设边界

- MCU: STM32H743IIT6。
- ADC 输入: PA0 / ADC1_INP16。
- DAC 输出: PA4 / DAC1_OUT1。
- 统一触发: TIM8 TRGO update event。
- 采样与输出频率: 200 kHz。
- 缓冲区长度: 1024 点，半缓冲 512 点。
- DAC 输出方式: 内部带 1.65V 偏置输出，示波器端允许 AC 耦合比较峰峰值。

## IOC/生成代码检查结论

当前 IOC 方向应保持:

| 项目 | 决策 | 说明 |
|---|---|---|
| ADC1 trigger | TIM8 TRGO | ADC1 与 DAC1 共用 TIM8 update event |
| DAC1 trigger | TIM8 TRGO | DAC1_CH1 / PA4 由 TIM8 触发更新 |
| TIM8 | Prescaler = 12-1, Period = 100-1 | 在当前 240 MHz 计数时钟假设下约 200 kHz |
| ADC1 DMA 初始模式 | DMA_NORMAL | 保持原建模采集逻辑，等待滤波命令后再切 circular |
| ADC1 ConversionDataManagement 初始模式 | ADC_CONVERSIONDATA_DMA_ONESHOT | 等待滤波命令后再切 `ADC_CONVERSIONDATA_DMA_CIRCULAR` |
| DAC1 DMA 初始模式 | DMA_CIRCULAR | DAC 原有逻辑不依赖 DAC；保持 circular 可减少开始滤波时的动态改动 |

## 运行态切换策略

系统默认处于等待命令状态。DAC 外设可以完成初始化，但在进入实时滤波前不启动真实 DAC DMA 输出。

收到开始滤波命令后，推荐顺序如下:

```text
1. Stop TIM8
2. Stop ADC DMA
3. 保持 DAC DMA 配置为 circular
4. 将 ADC1 ConversionDataManagement 切到 DMA_CIRCULAR
5. 将 ADC1 DMA Mode 切到 DMA_CIRCULAR
6. 预填 DAC 输出缓冲区为 1.65V 中点码或首块滤波结果
7. Start DAC DMA
8. Start ADC DMA
9. 清 TIM8 计数器
10. Start TIM8
11. 进入实时滤波运行态
```

退出实时滤波或回到建模采集时，应反向停止 TIM8、ADC DMA、DAC DMA，并根据目标模式恢复 ADC oneshot/normal 配置。

## 回调策略

后续代码实现必须区分两种模式:

- 建模采集模式: 使用现有完整缓冲回调，采满后 Stop DMA 并置完成标志。
- 实时滤波模式: 使用 Half Transfer 和 Transfer Complete 回调，分别处理前 512 点与后 512 点，不在回调内 Stop DMA。

不能让实时滤波模式复用当前 `HAL_ADC_ConvCpltCallback` 中直接 `HAL_ADC_Stop_DMA()` 的逻辑，否则 circular DMA 会在第一轮完成后被停止。

## 风险与验证

- TIM8 启动时刻决定 ADC/DAC 同步起点；开始滤波命令中应先停表并清计数，再启动 DMA，最后启动 TIM8。
- 若 TIM8 在等待命令阶段一直运行，启动 DMA 的第一个样本相位可能不可控。
- ADC DMA 切 circular 时必须同时处理 ADC 自身的 `ConversionDataManagement`，不能只改 DMA stream mode。
- STM32H743 DCache 可能导致 DMA 缓冲区数据不一致，后续实时缓冲区需要放非缓存区，或在 half/full 处理中做 cache invalidate/clean。

验证以示波器为主:

- PA4 输出应连续、无周期性断点。
- 两路示波器均 AC 耦合时，未知模型输出与探究装置输出峰峰值应满足题目误差要求。
- 半缓冲处理时间必须小于 512 / 200 kHz = 2.56 ms。
