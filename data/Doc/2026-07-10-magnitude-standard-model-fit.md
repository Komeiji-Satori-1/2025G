# 2026-07-10 Magnitude Standard Model Fit

## Goal

Create a MATLAB script to parse `data/RLC/data.txt`, plot the measured magnitude response, fit the magnitude data against standard second-order analog filter models, and print the fitted coefficients.

The script was later updated to batch-process the four RLC data files in the same format:

- `data/RLC/data.txt`
- `data/RLC/low-pass-data.txt`
- `data/RLC/high-pass-data.txt`
- `data/RLC/band-pass-data.txt`

## Files

- `data/RLC/fit_magnitude_standard_models.m`
- `data/RLC/data.txt`
- `data/RLC/low-pass-data.txt`
- `data/RLC/high-pass-data.txt`
- `data/RLC/band-pass-data.txt`
- `data/RLC/magnitude_standard_fit.csv`
- `data/RLC/magnitude_standard_fit.png`

## Method

The script parses rows in `freq,H_mag,H_phase` format and ignores non-numeric status lines.

It fits four standard models:

- low-pass
- high-pass
- band-pass
- band-stop

Each model is expressed as:

```text
H(s) = (n2*s^2 + n1*s + n0) / (s^2 + a*s + b)
a = w0 / Q
b = w0^2
w0 = 2*pi*f0
```

For each candidate `f0` and `Q`, the script solves the gain `K` by weighted least squares on magnitude only, then compares normalized squared magnitude error. Band-stop keeps notch points; the other models ignore very small measured magnitudes.

The script also applies endpoint shape rules before selecting the final model:

```text
LOW_PASS:  low-frequency end is passband, high-frequency end is stopband
HIGH_PASS: low-frequency end is stopband, high-frequency end is passband
BAND_PASS: low-frequency end is stopband, high-frequency end is stopband
BAND_STOP: low-frequency end is passband, high-frequency end is passband, with a notch
```

The rules use relative endpoint ratios instead of requiring absolute gain of `1`, because measured gain can include source, buffer, and ADC scaling errors. A passband endpoint must be at least `0.65 * peak_mag`; a stopband endpoint must be no more than `0.55 * peak_mag`.

## Verification

MATLAB batch run completed successfully.

Generated outputs:

```text
data/RLC/data_magnitude_standard_fit.csv
data/RLC/data_magnitude_standard_fit.png
data/RLC/low-pass-data_magnitude_standard_fit.csv
data/RLC/low-pass-data_magnitude_standard_fit.png
data/RLC/high-pass-data_magnitude_standard_fit.csv
data/RLC/high-pass-data_magnitude_standard_fit.png
data/RLC/band-pass-data_magnitude_standard_fit.csv
data/RLC/band-pass-data_magnitude_standard_fit.png
```

Parsed points:

```text
500
```

Best result:

```text
BAND_STOP
K  = 0.89746717999124648
f0 = 16033.539259351075 Hz
Q  = 0.5650565808421204
```

Standard analog coefficients:

```text
n2 = 0.89746717999124648
n1 = 0
n0 = 9108295486.99576
a  = 178286.03667672258
b  = 10148889775.651293
```

Firmware normalized analog coefficient format:

```text
b0 = 0.89746717999124647758
b1 = 0
b2 = 8.8430084455582991438e-11
a1 = 1.7567048279946588979e-05
a2 = 9.8532945189645264406e-11
```

Model errors:

```text
LOW_PASS  err = 0.483734709289
HIGH_PASS err = 0.185272612445
BAND_PASS err = 0.26993581104
BAND_STOP err = 0.00177316490681
```

## Notes

The measured minimum magnitude is near 16100 Hz, while the fitted band-stop center is about 16033.54 Hz. This is consistent with the measured magnitude curve.

The fitted notch is deeper than the measured minimum. That is expected for the ideal second-order band-stop model because its numerator has a zero exactly at `w0`; real measurement noise, sweep resolution, source/load effects, and ADC/DFT leakage prevent the measured notch from reaching zero.

This script only fits magnitude. It does not validate phase or time delay.

## Batch Results

`low-pass-data.txt`:

```text
Best model: LOW_PASS
K  = 0.88121927727573912
f0 = 16617.064891934951 Hz
Q  = 0.52507844640688295
err = 0.00074271372309
```

`high-pass-data.txt`:

```text
Best model: HIGH_PASS
K  = 1.0716699813182708
f0 = 1055.8144654088051 Hz
Q  = 0.1269979745917699
err = 0.00664461082191
```

For this file, the pure minimum-error result would be `BAND_PASS`, with error `0.00228456357552`. The endpoint shape rule rejects it because the high-frequency end remains close to the peak:

```text
low/peak  = 0.243932
high/peak = 0.860343
```

This is consistent with a high-pass response with a resonant voltage peak rather than a true band-pass response.

`band-pass-data.txt`:

```text
Best model: BAND_PASS
K  = 0.91771949622410576
f0 = 15950.083733658228 Hz
Q  = 0.55143220579223495
err = 0.00124828397924
```
