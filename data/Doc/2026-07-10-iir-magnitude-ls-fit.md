# 2026-07-10 IIR Magnitude Least-Squares Fit

## Task

Change only the IIR least-squares fitting stage to test whether magnitude-only fitting against standard second-order filter models can produce reasonable five analog coefficients.

## Files

- `IIR/iir.c`

## Changes

- `coef_calc()` now also stores valid sweep frequency and magnitude points for magnitude-only fitting.
- `matrix_calc()` now uses four standard second-order analog models for low-pass, high-pass, band-pass, and band-stop filters.
- Each model is fitted by weighted magnitude least squares.
- The best model is selected by least error and converted to firmware's existing five-coefficient analog format: `b0`, `b1`, `b2`, `a1`, `a2`.
- The original complex matrix-inversion implementation is retained inside the disabled `#else` branch for comparison.

## Design Notes

- This change does not modify the bilinear transform code.
- This change does not modify caller interfaces or existing function names.
- The standard denominator form is stable by construction: `s^2 + (w0/Q)s + w0^2`.
- The fitted analog coefficients use the existing firmware model: `H(s) = (b2*s^2 + b1*s + b0) / (a2*s^2 + a1*s + 1)`.

## Risks

- The current implementation uses a grid search over frequency and Q, so `matrix_calc()` is heavier than the previous 5x5 solve.
- The fit uses magnitude only, so the resulting phase is the standard model phase and does not include measurement-chain delay.
- The existing `get_filter_type()` still uses its old endpoint-threshold logic and may not match the best-fit model printed by `matrix_calc()`.

## Verification

- Static inspection only. Firmware build was not run in this environment.
