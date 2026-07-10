# 2026-07-10 IIR Fit From Raw Data MATLAB

## Task

Create a MATLAB script that computes second-order IIR coefficients directly from the raw measured transfer-function data.

## Files

- `data/RLC/fit_iir_from_raw_data.m`

## Changes

- Added a MATLAB script that parses `data/RLC/data.txt` and fits a second-order analog transfer function.
- Uses normalized frequency in the least-squares problem to avoid the large `w^4` conditioning issue.
- Converts the fitted analog transfer function to digital IIR coefficients using a hand-written bilinear transform.
- Prints analog coefficients, `Solution N`, floating-point digital coefficients, Q45-like integer coefficients, and pole stability checks.
- Exports `data/RLC/iir_fit_from_raw_data.csv` when it runs.

## Notes

- The script uses the complex measured response `H = real + j*imag`.
- It excludes frequencies above `0.45*Fs_iir` and points with very small measured magnitude by default.
- The script is intended for offline verification before changing the firmware fitting code.
