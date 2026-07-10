# 2026-07-10 IIR Filter Type Endpoint Rules

## Goal

Avoid selecting a physically wrong second-order filter type when a lower least-squares magnitude error is caused by finite sweep range or resonant peaking.

The immediate case is a passive series RLC high-pass response. Its high-frequency end stays near the passband level, but a broad band-pass model can produce a slightly lower magnitude-only fitting error.

## Files

- `IIR/iir.c`

## Change

`matrix_calc()` still fits four standard analog models:

- low-pass
- high-pass
- band-pass
- band-stop

It now also calculates endpoint shape features from the measured magnitude data:

```text
low/peak    = average magnitude of the first 5% points / peak magnitude
high/peak   = average magnitude of the last 10% points / peak magnitude
trough/peak = minimum magnitude / peak magnitude
```

The final type is selected from models that pass endpoint shape rules:

```text
LOW_PASS:  low-frequency end is passband, high-frequency end is stopband
HIGH_PASS: low-frequency end is stopband, high-frequency end is passband
BAND_PASS: low-frequency end is stopband, high-frequency end is stopband
BAND_STOP: low-frequency end is passband, high-frequency end is passband, with a notch
```

Thresholds:

```text
FIT_PASS_END_MIN_RATIO = 0.65
FIT_STOP_END_MAX_RATIO = 0.55
```

If no model passes the endpoint rules, the code falls back to the previous behavior: choose the minimum fitting error.

## UART Print

The firmware now prints:

```text
Endpoint shape: low/peak = ..., high/peak = ..., trough/peak = ...
LP error = ..., shape_ok = ...
HP error = ..., shape_ok = ...
BP error = ..., shape_ok = ...
BS error = ..., shape_ok = ...
```

This makes it visible why a model was accepted or rejected.

## Notes

The endpoint rules use ratios instead of absolute gain of `1.0`, because source amplitude, buffer gain, ADC scaling, and measurement calibration can move the measured passband away from exactly one.

For a series RLC high-pass response measured across the inductor, a resonant voltage peak can exceed the input voltage while the high-frequency end still remains near the passband level. That curve should be classified as high-pass rather than band-pass.
