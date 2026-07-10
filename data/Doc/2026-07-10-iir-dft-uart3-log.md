# 2026-07-10 IIR DFT UART3 Log

## Task

Print each sweep frequency point's DFT and transfer-function result through USART3 so the measured curve can be fitted and inspected offline.

## Files

- `Myfun/calculate.c`

## Changes

- Added one CSV header when `calculate_learn_start()` begins a learning sweep.
- Added one CSV data row after each `FFT_CalcTransfer()` call in `LEARN_PROCESS_FFT`.
- Printed fields: index, frequency, input DFT real/imag/magnitude/phase, output DFT real/imag/magnitude/phase, and transfer function real/imag/magnitude/phase.

## Design Notes

- Existing `printf` is already redirected to USART3 by `Core/Src/usart.c`, so no UART routing change was needed.
- The data row is emitted after the transfer function is calculated and before advancing to the next sweep frequency.
- Existing function names, variables, and module interfaces were kept unchanged.

## Risks

- At 115200 baud, 500 CSV rows will add several seconds of blocking UART transmit time during learning.
- Float formatting depends on the current C library printf-float configuration, which is already used elsewhere in the IIR code.

## Verification

- Code was reviewed by inspection. Firmware build was not run in this environment.
