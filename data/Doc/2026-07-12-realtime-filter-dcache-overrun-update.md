# 2026-07-12 实时滤波 DCache / overrun 优化记录

## 目标
参考 EDU_Work 的实时链路方案，降低 STM32H7 实时 DAC 输出中的旧数据、虚影和偶发掉点风险。

## 修改范围
- `Myfun/realtime_filter.c`
- `Myfun/realtime_filter.h`
- `Myfun/state.c`
- `Myfun/modify_adc.c`
- `Myfun/modify_adc.h`
- `Core/Src/main.c`
- `Core/Inc/adc.h`
- `Core/Src/usart.c`
- `Core/Inc/usart.h`
- `Core/Src/adc.c`
- `Core/Src/dac.c`
- `Core/Src/dma.c`
- `25G.ioc`

## 关键改动
- 为实时 ADC/DAC 缓冲增加 DCache 维护。
- 启动前清空 DAC 缓冲并 clean cache。
- 启动前对 ADC 缓冲 clean+invalidate。
- 每次处理半缓冲前 invalidate ADC 区域，处理后 clean DAC 区域。
- 增加 `iir_overrun_count`，在 half/full 回调里检测同一半缓冲重复到达。
- 主循环改为每轮只处理一个 pending 半缓冲。
- 实时阶段通过 `App_Printf_SetEnabled(0U)` 禁止 `printf` 阻塞。
- 实时滤波重配置时将 ADC1 DMA 传输优先级提升到 `DMA_PRIORITY_VERY_HIGH`。
- DMA IRQ 的 NVIC priority 调整为 1，保留 USART1 priority 0，使屏幕串口中断可抢占 DMA 中断。

## 设计决策
- 保持现有函数名和模块边界，不重构状态机。
- DCache 处理只覆盖实时滤波缓冲，不扩散到学习模式缓冲，降低改动面。
- CubeMX 生成配置保留原有 ADC LOW / DAC HIGH 的 DMA 传输优先级，避免全局提高 DMA 优先级影响 HMI。
- overrun 只做计数，不改变实时数据路径，便于先定位瓶颈再决定后续降块长或改错位输出。

## 风险
- `SCB_*DCache*` 依赖缓存行对齐；当前实时缓冲已做 32 字节对齐，长度也是 32 字节整数倍。
- `printf` 在实时运行期间被抑制，若其他模块误打日志将静默丢弃。

## 验证状态
- 已完成静态代码回看。
- 还未在板上做编译和波形验证。
