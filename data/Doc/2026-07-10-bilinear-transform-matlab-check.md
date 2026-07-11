# 2026-07-10 Bilinear Transform MATLAB Check

## Task

Create a MATLAB script that verifies the bilinear transform independently from the firmware implementation.

## Files

- `data/RLC/check_bilinear_transform.m`

## Changes

- Added a standalone MATLAB script with a hand-written second-order bilinear transform.
- The script imports `data/RLC/data.txt`, compares measured response, analog fitted response, and digital IIR response.
- The script prints analog/digital coefficients, zeros, poles, and stability warnings.
- The script exports `data/RLC/bilinear_iir_compare.csv` when it runs.

## Notes

- The script uses the same model as firmware: `H(s) = (b2*s^2 + b1*s + b0) / (a2*s^2 + a1*s + 1)`.
- The transform used is `s = 2*Fs*(1 - z^-1)/(1 + z^-1)`.
