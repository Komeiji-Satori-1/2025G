# 2026-07-10 RLC MATLAB Response Plot

## Task

Create a MATLAB script that parses `data/RLC/data.txt` and plots the measured transfer-function response.

## Files

- `data/RLC/plot_rlc_response.m`

## Changes

- Added a MATLAB script that extracts `Freq` and `H:` records from the text log.
- Plots magnitude, magnitude in dB, wrapped phase, unwrapped phase, real part, and imaginary part.
- Exports a cleaned CSV file named `data/RLC/rlc_response_parsed.csv` when the script runs.

## Notes

- The script ignores `Input:`, `Output:`, IIR coefficient, and status lines.
- It keeps the measured `H_real` and `H_imag` from the log rather than reconstructing them from magnitude and phase.
