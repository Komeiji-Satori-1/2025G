clearvars -except inputFile inputFiles;
clc;
close all;

scriptDir = fileparts(mfilename('fullpath'));
if exist('inputFiles', 'var')
    dataFiles = inputFiles;
elseif exist('inputFile', 'var')
    dataFiles = {inputFile};
else
    dataFiles = {
        fullfile(scriptDir, 'NEWDATA.txt')
    };
end

Fs_iir = 200000;

validMinMagRate = 0.025;
passEndMinRatio = 0.60;
stopEndMaxRatio = 0.55;
notchSideMinRatio = 0.45;
qMin = 0.10;
qMax = 20.0;
coarseF0Count = 160;
coarseQCount = 80;
refineF0Count = 80;
refineQCount = 60;
refineSpanRate = [0.18, 0.05];

for fileIndex = 1:numel(dataFiles)
    dataFile = dataFiles{fileIndex};
    if ~isfile(dataFile)
        fprintf('Skip missing file: %s\n', dataFile);
        continue;
    end

    fitMagnitudeFile(dataFile, validMinMagRate, qMin, qMax, ...
        coarseF0Count, coarseQCount, refineF0Count, refineQCount, refineSpanRate, ...
        passEndMinRatio, stopEndMaxRatio, notchSideMinRatio);
end

function fitMagnitudeFile(dataFile, validMinMagRate, qMin, qMax, ...
    coarseF0Count, coarseQCount, refineF0Count, refineQCount, refineSpanRate, ...
    passEndMinRatio, stopEndMaxRatio, notchSideMinRatio)

[freq, mag, phase] = parseMagnitudeCsv(dataFile);

peakMag = max(mag);
validMinMag = peakMag * validMinMagRate;
shape = calcShapeFeatures(mag);

modelNames = {'LOW_PASS', 'HIGH_PASS', 'BAND_PASS', 'BAND_STOP'};
fitResults = repmat(emptyFitResult(), 1, numel(modelNames));

for modelIndex = 1:numel(modelNames)
    fitResults(modelIndex) = fitOneModel(modelNames{modelIndex}, freq, mag, ...
        validMinMag, qMin, qMax, coarseF0Count, coarseQCount, ...
        refineF0Count, refineQCount, refineSpanRate);
end

shapeAllowed = false(1, numel(fitResults));
for modelIndex = 1:numel(fitResults)
    shapeAllowed(modelIndex) = isShapeAllowed(fitResults(modelIndex).type, shape, ...
        passEndMinRatio, stopEndMaxRatio, notchSideMinRatio);
end

if any(shapeAllowed)
    allowedErr = [fitResults.err];
    allowedErr(~shapeAllowed) = Inf;
    [~, bestIndex] = min(allowedErr);
else
    [~, bestIndex] = min([fitResults.err]);
    fprintf('Warning: no model passed endpoint shape rules, using minimum error only.\n');
end
best = fitResults(bestIndex);

fprintf('Parsed %d points from %s\n', numel(freq), dataFile);
fprintf('Peak magnitude = %.12g, valid_min_mag = %.12g\n\n', peakMag, validMinMag);
fprintf(['Endpoint shape: low/peak=%.6f, high/peak=%.6f, ', ...
    'trough/peak=%.6f, left_peak/peak=%.6f, right_peak/peak=%.6f\n\n'], ...
    shape.lowRatio, shape.highRatio, shape.troughRatio, ...
    shape.leftPeakRatio, shape.rightPeakRatio);

for modelIndex = 1:numel(fitResults)
    r = fitResults(modelIndex);
    fprintf('%s: err=%.12g, shape_ok=%d, K=%.12g, f0=%.6f Hz, Q=%.12g\n', ...
        r.type, r.err, shapeAllowed(modelIndex), r.K, r.f0, r.Q);
end

[n2, n1, n0, a, b] = standardCoefficients(best.type, best.K, best.f0, best.Q);
[fw_b0, fw_b1, fw_b2, fw_a1, fw_a2] = firmwareCoefficients(n2, n1, n0, a, b);

fprintf('\nBest model: %s\n', best.type);
fprintf('Standard analog model:\n');
fprintf('  H(s) = (n2*s^2 + n1*s + n0) / (s^2 + a*s + b)\n');
fprintf('  n2 = %.17g\n', n2);
fprintf('  n1 = %.17g\n', n1);
fprintf('  n0 = %.17g\n', n0);
fprintf('  a  = %.17g\n', a);
fprintf('  b  = %.17g\n', b);
fprintf('  K  = %.17g\n', best.K);
fprintf('  f0 = %.17g Hz\n', best.f0);
fprintf('  Q  = %.17g\n', best.Q);

fprintf('\nFirmware analog coefficient format:\n');
fprintf('  H(s) = (b2*s^2 + b1*s + b0) / (a2*s^2 + a1*s + 1)\n');
fprintf('  b0 = %.20g\n', fw_b0);
fprintf('  b1 = %.20g\n', fw_b1);
fprintf('  b2 = %.20g\n', fw_b2);
fprintf('  a1 = %.20g\n', fw_a1);
fprintf('  a2 = %.20g\n', fw_a2);

[~, dataName, ~] = fileparts(dataFile);
fitMag = modelMagnitude(best.type, best.K, best.f0, best.Q, freq);

outputCsv = fullfile(fileparts(dataFile), [dataName, '_magnitude_standard_fit.csv']);
resultTable = table(freq, mag, phase, fitMag, ...
    'VariableNames', {'freq_Hz', 'measured_mag', 'measured_phase_rad', 'fit_mag'});
writetable(resultTable, outputCsv);
fprintf('\nFit CSV written to: %s\n', outputCsv);

figure('Name', ['Magnitude Standard Model Fit - ', dataName], 'Color', 'w');
subplot(2, 1, 1);
plot(freq, mag, 'k.', 'DisplayName', 'Measured');
hold on;
plot(freq, fitMag, 'r-', 'LineWidth', 1.5, 'DisplayName', best.type);
grid on;
xlabel('Frequency / Hz');
ylabel('|H|');
title(['Magnitude Response - ', dataName], 'Interpreter', 'none');
legend('Location', 'best', 'Interpreter', 'none');

subplot(2, 1, 2);
plot(freq, 20 * log10(max(mag, eps)), 'k.', 'DisplayName', 'Measured');
hold on;
plot(freq, 20 * log10(max(fitMag, eps)), 'r-', 'LineWidth', 1.5, 'DisplayName', best.type);
grid on;
xlabel('Frequency / Hz');
ylabel('Magnitude / dB');
title(['Magnitude Response in dB - ', dataName], 'Interpreter', 'none');
legend('Location', 'best', 'Interpreter', 'none');

plotFile = fullfile(fileparts(dataFile), [dataName, '_magnitude_standard_fit.png']);
saveas(gcf, plotFile);
fprintf('Fit plot written to: %s\n', plotFile);
fprintf('\n');
end

function [freq, mag, phase] = parseMagnitudeCsv(dataFile)
    lines = readlines(dataFile);
    freq = [];
    mag = [];
    phase = [];

    for i = 1:numel(lines)
        line = strtrim(lines(i));
        values = sscanf(line, '%f,%f,%f');
        if numel(values) == 3
            freq(end + 1, 1) = values(1); %#ok<AGROW>
            mag(end + 1, 1) = values(2); %#ok<AGROW>
            phase(end + 1, 1) = values(3); %#ok<AGROW>
        end
    end

    if isempty(freq)
        error('No numeric freq,mag,phase rows were parsed from %s', dataFile);
    end

    [freq, order] = sort(freq);
    mag = mag(order);
    phase = phase(order);
end

function r = emptyFitResult()
    r = struct('type', '', 'err', Inf, 'K', 0, 'f0', 0, 'Q', 0);
end

function shape = calcShapeFeatures(mag)
    pointCount = numel(mag);
    lowCount = max(1, round(pointCount * 0.05));
    highCount = max(1, round(pointCount * 0.10));
    peakMag = max(mag);
    [troughMag, troughIndex] = min(mag);

    shape.lowAvg = mean(mag(1:lowCount));
    shape.highAvg = mean(mag(pointCount - highCount + 1:pointCount));
    shape.peakMag = peakMag;
    shape.troughMag = troughMag;
    shape.leftPeakMag = 0;
    shape.rightPeakMag = 0;

    if troughIndex > 1
        shape.leftPeakMag = max(mag(1:troughIndex - 1));
    end

    if troughIndex < pointCount
        shape.rightPeakMag = max(mag(troughIndex + 1:pointCount));
    end

    shape.lowRatio = shape.lowAvg / max(peakMag, eps);
    shape.highRatio = shape.highAvg / max(peakMag, eps);
    shape.troughRatio = shape.troughMag / max(peakMag, eps);
    shape.leftPeakRatio = shape.leftPeakMag / max(peakMag, eps);
    shape.rightPeakRatio = shape.rightPeakMag / max(peakMag, eps);
end

function allowed = isShapeAllowed(type, shape, passEndMinRatio, stopEndMaxRatio, notchSideMinRatio)
    lowPassEnd = shape.lowRatio >= passEndMinRatio;
    highPassEnd = shape.highRatio >= passEndMinRatio;
    lowStopEnd = shape.lowRatio <= stopEndMaxRatio;
    highStopEnd = shape.highRatio <= stopEndMaxRatio;
    hasNotchBetweenPassbands = (shape.troughRatio <= stopEndMaxRatio) && ...
        (shape.leftPeakRatio >= notchSideMinRatio) && ...
        (shape.rightPeakRatio >= notchSideMinRatio);

    switch type
        case 'LOW_PASS'
            allowed = lowPassEnd && highStopEnd;
        case 'HIGH_PASS'
            allowed = lowStopEnd && highPassEnd;
        case 'BAND_PASS'
            allowed = lowStopEnd && highStopEnd;
        case 'BAND_STOP'
            allowed = (lowPassEnd && highPassEnd && (shape.troughRatio <= stopEndMaxRatio)) || ...
                hasNotchBetweenPassbands;
        otherwise
            allowed = false;
    end
end

function best = fitOneModel(type, freq, mag, validMinMag, qMin, qMax, ...
    coarseF0Count, coarseQCount, refineF0Count, refineQCount, refineSpanRate)

    best = emptyFitResult();
    best.type = type;

    fMin = min(freq);
    fMax = max(freq);
    qCandidates = logspace(log10(qMin), log10(qMax), coarseQCount);
    fCandidates = linspace(fMin, fMax, coarseF0Count);

    best = searchGrid(type, freq, mag, validMinMag, fCandidates, qCandidates, best);

    for roundIndex = 1:numel(refineSpanRate)
        span = refineSpanRate(roundIndex);
        fLow = max(fMin, best.f0 * (1.0 - span));
        fHigh = min(fMax, best.f0 * (1.0 + span));
        qLow = max(qMin, best.Q / (1.0 + 4.0 * span));
        qHigh = min(qMax, best.Q * (1.0 + 4.0 * span));

        fCandidates = linspace(fLow, fHigh, refineF0Count);
        qCandidates = logspace(log10(qLow), log10(qHigh), refineQCount);
        best = searchGrid(type, freq, mag, validMinMag, fCandidates, qCandidates, best);
    end
end

function best = searchGrid(type, freq, mag, validMinMag, fCandidates, qCandidates, best)
    for fi = 1:numel(fCandidates)
        f0 = fCandidates(fi);

        if f0 <= 0
            continue;
        end

        for qi = 1:numel(qCandidates)
            Q = qCandidates(qi);
            base = modelMagnitude(type, 1.0, f0, Q, freq);
            denom = max(mag, validMinMag);
            weight = 1 ./ (denom .* denom);

            if strcmp(type, 'BAND_STOP')
                active = isfinite(base) & isfinite(mag) & (base >= 0);
            else
                active = isfinite(base) & isfinite(mag) & (base >= 0) & (mag >= validMinMag);
            end

            if nnz(active) < 5
                continue;
            end

            kDen = sum(weight(active) .* base(active) .* base(active));
            if kDen <= eps
                continue;
            end

            K = sum(weight(active) .* mag(active) .* base(active)) / kDen;
            if K <= 0
                continue;
            end

            residual = (K .* base(active) - mag(active)) ./ denom(active);
            err = mean(residual .* residual);

            if err < best.err
                best.err = err;
                best.K = K;
                best.f0 = f0;
                best.Q = Q;
            end
        end
    end
end

function m = modelMagnitude(type, K, f0, Q, freq)
    w = 2 * pi * freq(:);
    w0 = 2 * pi * f0;
    a = w0 / Q;
    b = w0 * w0;

    den = sqrt((b - w .* w).^2 + (a .* w).^2);

    switch type
        case 'LOW_PASS'
            num = b * ones(size(w));
        case 'HIGH_PASS'
            num = w .* w;
        case 'BAND_PASS'
            num = a .* w;
        case 'BAND_STOP'
            num = abs(b - w .* w);
        otherwise
            error('Unknown model type: %s', type);
    end

    m = K .* num ./ max(den, eps);
end

function [n2, n1, n0, a, b] = standardCoefficients(type, K, f0, Q)
    w0 = 2 * pi * f0;
    a = w0 / Q;
    b = w0 * w0;
    n2 = 0;
    n1 = 0;
    n0 = 0;

    switch type
        case 'LOW_PASS'
            n0 = K * b;
        case 'HIGH_PASS'
            n2 = K;
        case 'BAND_PASS'
            n1 = K * a;
        case 'BAND_STOP'
            n2 = K;
            n0 = K * b;
        otherwise
            error('Unknown model type: %s', type);
    end
end

function [fw_b0, fw_b1, fw_b2, fw_a1, fw_a2] = firmwareCoefficients(n2, n1, n0, a, b)
    fw_b0 = n0 / b;
    fw_b1 = n1 / b;
    fw_b2 = n2 / b;
    fw_a1 = a / b;
    fw_a2 = 1 / b;
end
