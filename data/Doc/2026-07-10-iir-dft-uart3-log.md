# 2026-07-10 IIR DFT UART3 Log

## Task

Print each sweep frequency point's transfer-function magnitude and phase through USART3 so the measured curve can be fitted and inspected offline.

## Files

- `Myfun/calculate.c`

## Changes

- Added one CSV header when `calculate_learn_start()` begins a learning sweep.
- Added one CSV data row after each `FFT_CalcTransfer()` call in `LEARN_PROCESS_FFT`.
- Printed only the minimum fitting fields: frequency, transfer-function magnitude, and transfer-function phase.

## Design Notes

- Existing `printf` is already redirected to USART3 by `Core/Src/usart.c`, so no UART routing change was needed.
- The data row is emitted after the transfer function is calculated and before advancing to the next sweep frequency.
- Magnitude and phase are enough to reconstruct the complex response offline as `H_re = H_mag * cos(H_phase)` and `H_im = H_mag * sin(H_phase)`.
- Existing function names, variables, and module interfaces were kept unchanged.

## Risks

- At 115200 baud, 500 CSV rows still add blocking UART transmit time during learning, but the reduced three-column format keeps it small.
- Float formatting depends on the current C library printf-float configuration, which is already used elsewhere in the IIR code.

## Verification

- Code was reviewed by inspection. Firmware build was not run in this environment.
