# 2026-07-11 IIR Use Fitted Filter Type

## Goal

Stop using the old `get_filter_type()` coefficient-threshold decision as the final filter type result.

The old logic used absolute endpoint thresholds such as `|H(0)| > 0.8`, which fails when the passband gain is not close to `1.0`. In the observed band-stop case, the magnitude fit selected `BAND_STOP_FILTER`, but the old endpoint threshold logic displayed `BAND_PASS_FILTER`.

## Files

- `IIR/iir.h`
- `IIR/iir.c`
- `Myfun/calculate.c`

## Changes

- Added `last_fit_filter_type` inside `IIR/iir.c`.
- `matrix_calc()` now stores `best.type` into `last_fit_filter_type`.
- Added:

```c
FILTER_TYPE get_last_fit_filter_type(void);
void show_filter_type(FILTER_TYPE type);
```

- `Myfun/calculate.c` now displays the fitted type:

```c
show_filter_type(get_last_fit_filter_type());
```

instead of:

```c
get_filter_type(&analog_coef_data);
```

## Threshold Updates

The magnitude fit thresholds were adjusted:

```text
FIT_Q_MIN = 0.10
FIT_PASS_END_MIN_RATIO = 0.60
FIT_STOP_END_MAX_RATIO = 0.55
```

The lower `FIT_Q_MIN` allows broad low-Q RLC responses such as `Q ~= 0.16`. The lower pass-end ratio prevents a valid band-stop response with `high/peak ~= 0.638` from being rejected by endpoint shape rules.

## 2026-07-11 Notch Near Sweep Edge Update

A later `NEWDATA.txt` case exposed another edge case:

```text
low/peak = 0.124599
high/peak = 0.507484
trough/peak = 0.001406
left_peak/peak = 1.000000
right_peak/peak = 0.533313
```

The notch is close to the low-frequency sweep edge, so the first 5% endpoint average is pulled down by the notch. The previous endpoint-only band-stop rule rejected this case, allowing a band-pass model to win.

The rule now also accepts band-stop when:

```text
trough/peak <= FIT_STOP_END_MAX_RATIO
left_peak/peak >= FIT_NOTCH_SIDE_MIN_RATIO
right_peak/peak >= FIT_NOTCH_SIDE_MIN_RATIO
```

with:

```text
FIT_NOTCH_SIDE_MIN_RATIO = 0.45
```

This checks whether there is a real notch with recovery on both sides, rather than relying only on endpoint averages.

## Notes

The old `get_filter_type()` implementation is no longer part of the learn flow. It should not be used as the final filter type source.
