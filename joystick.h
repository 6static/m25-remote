/*
 * joystick.h - Joystick reading, calibration, deadzone, normalization
 *
 * Reads the analog X/Y axes of a 2-axis joystick (e.g. KY-023).
 * Provides normalized values in [-1.0, +1.0] with deadzone applied.
 *
 * Coordinate convention:
 *   X: negative = left,   positive = right
 *   Y: negative = backward, positive = forward
 */

#ifndef JOYSTICK_H
#define JOYSTICK_H

#include <Arduino.h>
#include "device_config.h"
#include "Logger.h"

#ifdef NO_JOYSTICK
 // ---------------------------------------------------------------------------
 // Stub implementation: no hardware joystick connected.
 // All reads return a perfectly centered, in-deadzone state.
 // ---------------------------------------------------------------------------
inline void joystickInit() {
    Logger::instance().logForced(LogLevel::INFO, TAG_JOYSTICK, __FILE__, __LINE__, "NO_JOYSTICK defined - ADC disabled");
}
inline bool joystickRecalibrate() {
    Logger::instance().logForced(LogLevel::INFO, TAG_JOYSTICK, __FILE__, __LINE__, "NO_JOYSTICK - recal skipped");
    return true;
}
inline int  joystickReadRawAxis(uint8_t) { return JOYSTICK_CENTER; }
struct JoystickRaw { int x = JOYSTICK_CENTER; int y = JOYSTICK_CENTER; };
inline bool joystickRawNearExpectedCenter(const JoystickRaw&) { return true; }
inline bool joystickHasRangeCalibration() { return true; }
inline void joystickSetDeadzonePct(uint8_t) {}
inline uint8_t joystickGetDeadzonePct() { return JOYSTICK_DEADZONE_PCT; }
struct JoystickNorm { float x = 0.0f; float y = 0.0f; bool inDeadzone = true; };
inline const char* joystickDirectionLabel(float, float) { return "CENTER"; }
inline JoystickRaw  joystickReadRaw() { return { JOYSTICK_CENTER, JOYSTICK_CENTER }; }
inline JoystickNorm joystickRead() { return { 0.0f, 0.0f, true }; }
inline JoystickNorm joystickNormalize(const JoystickRaw&) { return { 0.0f, 0.0f, true }; }
inline void joystickSetMapping(bool, bool, bool) {}
#else // real joystick below

 // ---------------------------------------------------------------------------
 // Types
 // ---------------------------------------------------------------------------
struct JoystickRaw {
    int x;   // ADC counts 0-4095
    int y;
};

struct JoystickNorm {
    float x;   // -1.0 (left/backward) ... +1.0 (right/forward)
    float y;
    bool  inDeadzone;  // true if both axes are within deadzone
};

// ---------------------------------------------------------------------------
// Internal state
// NOTE: static file-scope variables in a header give each translation unit
// that includes this file its own independent copy of the calibration state.
// joystick.h must therefore be included from exactly one TU (the main .ino).
// ---------------------------------------------------------------------------
static int _jsXCenter = JOYSTICK_CENTER;
static int _jsYCenter = JOYSTICK_CENTER;
// Full-range calibration endpoints. Default to theoretical ADC range.
// Updated by joystickApplyFullRangeCalibration() after NVS load or 'cal full'.
static int _jsXMin = JOYSTICK_ADC_MIN;
static int _jsXMax = JOYSTICK_ADC_MAX;
static int _jsYMin = JOYSTICK_ADC_MIN;
static int _jsYMax = JOYSTICK_ADC_MAX;
static bool _jsRangeCalibrated = false;
// Wiring map learned by 'cal full'. Centers and ranges are stored in logical axes.
static bool _jsSwapXY = false;   // forward/back is wired to JOYSTICK_X_PIN
static bool _jsInvertX = false;  // pushing left raises the ADC value
static bool _jsInvertY = false;  // pushing forward lowers the ADC value
static uint8_t _jsDeadzonePct = JOYSTICK_DEADZONE_PCT;

inline bool joystickHasRangeCalibration() { return _jsRangeCalibrated; }
inline uint8_t joystickGetDeadzonePct() { return _jsDeadzonePct; }
inline void joystickSetDeadzonePct(uint8_t pct) { _jsDeadzonePct = pct; }

// ---------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------
inline void joystickInit() {
    analogReadResolution(ADC_RESOLUTION_BITS);
    analogSetPinAttenuation(JOYSTICK_X_PIN, ADC_11db);
    analogSetPinAttenuation(JOYSTICK_Y_PIN, ADC_11db);
    const int8_t gpioX = digitalPinToGPIONumber(JOYSTICK_X_PIN);
    const int8_t gpioY = digitalPinToGPIONumber(JOYSTICK_Y_PIN);
    const int8_t chX = digitalPinToAnalogChannel(JOYSTICK_X_PIN);
    const int8_t chY = digitalPinToAnalogChannel(JOYSTICK_Y_PIN);
    Logger::instance().logForced(LogLevel::INFO, TAG_JOYSTICK, __FILE__, __LINE__,
        "Pin map: X pin %d->gpio %d ch %d, Y pin %d->gpio %d ch %d",
        JOYSTICK_X_PIN, gpioX, chX, JOYSTICK_Y_PIN, gpioY, chY);
    if (chX < 0 || chY < 0) {
        Logger::instance().logForced(LogLevel::ERROR, TAG_JOYSTICK, __FILE__, __LINE__,
            "Configured joystick pins are not ADC-capable on this board variant");
    }

    // Warm-up reads reduce first-sample artifacts after attenuation changes.
    for (int i = 0; i < 4; i++) {
        (void)analogRead(JOYSTICK_X_PIN);
        (void)analogRead(JOYSTICK_Y_PIN);
        delay(2);
    }

    const int bootProbeX = analogRead(JOYSTICK_X_PIN);
    const int bootProbeY = analogRead(JOYSTICK_Y_PIN);
    Logger::instance().logForced(LogLevel::INFO, TAG_JOYSTICK, __FILE__, __LINE__,
        "Boot ADC probe: X=%d Y=%d (pins %d/%d)",
        bootProbeX, bootProbeY, JOYSTICK_X_PIN, JOYSTICK_Y_PIN);

    if (bootProbeX == 0 && bootProbeY == 0) {
        Logger::instance().logForced(LogLevel::WARN, TAG_JOYSTICK, __FILE__, __LINE__,
            "Boot probe is hard-zero on both axes - check joystick VCC/GND and signal wiring");
    }

    // Calibrate center by averaging ADC samples taken at rest
    const int calSamples = 64;
    long sumX = 0, sumY = 0;
    int minX = JOYSTICK_ADC_MAX;
    int maxX = JOYSTICK_ADC_MIN;
    int minY = JOYSTICK_ADC_MAX;
    int maxY = JOYSTICK_ADC_MIN;
    for (int i = 0; i < calSamples; i++) {
        int x = analogRead(JOYSTICK_X_PIN);
        int y = analogRead(JOYSTICK_Y_PIN);
        sumX += x;
        sumY += y;
        if (x < minX) minX = x;
        if (x > maxX) maxX = x;
        if (y < minY) minY = y;
        if (y > maxY) maxY = y;
        delay(4);
    }
    _jsXCenter = (int)(sumX / calSamples);
    _jsYCenter = (int)(sumY / calSamples);
    Logger::instance().logForced(LogLevel::INFO, TAG_JOYSTICK, __FILE__, __LINE__,
        "Center calibrated: X=%d Y=%d", _jsXCenter, _jsYCenter);
    Logger::instance().logForced(LogLevel::INFO, TAG_JOYSTICK, __FILE__, __LINE__,
        "Boot ADC span: X=%d..%d (d=%d) Y=%d..%d (d=%d)",
        minX, maxX, (maxX - minX), minY, maxY, (maxY - minY));

    const bool xClamped = (maxX >= (JOYSTICK_ADC_MAX - 2)) || (minX <= (JOYSTICK_ADC_MIN + 2));
    const bool yClamped = (maxY >= (JOYSTICK_ADC_MAX - 2)) || (minY <= (JOYSTICK_ADC_MIN + 2));
    if (xClamped || yClamped) {
        Logger::instance().logForced(LogLevel::WARN, TAG_JOYSTICK, __FILE__, __LINE__,
            "ADC near rail during boot calibration (check wiring/3.3V/GND, attenuation, and pin mapping)");
    }
}

// ---------------------------------------------------------------------------
// Low-level: averaged ADC read
// ---------------------------------------------------------------------------
inline int joystickReadRawAxis(uint8_t pin) {
    const int readSamples = 8;
    long sum = 0;
    for (int i = 0; i < readSamples; i++) {
        sum += analogRead(pin);
    }
    return (int)(sum / readSamples);
}

// Physical pins: x = JOYSTICK_X_PIN, y = JOYSTICK_Y_PIN, before the wiring map
inline JoystickRaw joystickReadPins() {
    return { joystickReadRawAxis(JOYSTICK_X_PIN),
             joystickReadRawAxis(JOYSTICK_Y_PIN) };
}

// Logical axes: x = left/right, y = forward/back (swap applied, invert is applied in normalize)
inline JoystickRaw joystickReadRaw() {
    JoystickRaw p = joystickReadPins();
    JoystickRaw r = _jsSwapXY ? JoystickRaw{ p.y, p.x } : p;
#ifdef JOYSTICK_FWD_BWD_ONLY
    r.x = _jsXCenter;
#endif
    return r;
}

// Logical centers expressed per physical pin, for cal which works on pins
inline JoystickRaw joystickPinCenter() {
    return _jsSwapXY ? JoystickRaw{ _jsYCenter, _jsXCenter } : JoystickRaw{ _jsXCenter, _jsYCenter };
}

inline void joystickSetMapping(bool swapXY, bool invertX, bool invertY) {
    if (swapXY != _jsSwapXY) {
        int t = _jsXCenter; _jsXCenter = _jsYCenter; _jsYCenter = t;
        t = _jsXMin; _jsXMin = _jsYMin; _jsYMin = t;
        t = _jsXMax; _jsXMax = _jsYMax; _jsYMax = t;
    }
    _jsSwapXY = swapXY;
    _jsInvertX = invertX;
    _jsInvertY = invertY;
}

inline bool joystickMappingSwapped() { return _jsSwapXY; }
inline bool joystickMappingInvertX() { return _jsInvertX; }
inline bool joystickMappingInvertY() { return _jsInvertY; }

// ---------------------------------------------------------------------------
// Normalize one axis to [-1.0, +1.0] using measured range endpoints.
// adcMin/adcMax are the actual min/max observed during full-range calibration
// (or the theoretical 0/4095 before calibration is done).
// Returns 0.0 if within deadzone.
// ---------------------------------------------------------------------------
inline int _jsSideDeadzone(int travel) {
    int dz = travel * _jsDeadzonePct / 100;
    return (dz < JOYSTICK_DEADZONE_MIN_COUNTS) ? JOYSTICK_DEADZONE_MIN_COUNTS : dz;
}

inline float joystickNormalizeAxis(int raw, int center, int adcMin, int adcMax) {
    int offset = raw - center;

    // Remap outside deadzone to full [-1, +1] using measured endpoints.
    // Per-side deadzone: the rest center is rarely mid-range.
    float usable;
    if (offset > 0) {
        int dz = _jsSideDeadzone(adcMax - center);
        if (offset <= dz) return 0.0f;
        int edgePos = center + dz;
        int rangePos = adcMax - edgePos;
        if (rangePos <= 0) return 1.0f;
        usable = (float)(raw - edgePos) / (float)rangePos;
    }
    else {
        int dz = _jsSideDeadzone(center - adcMin);
        if (-offset <= dz) return 0.0f;
        int edgeNeg = center - dz;
        int rangeNeg = edgeNeg - adcMin;
        if (rangeNeg <= 0) return -1.0f;
        usable = -((float)(edgeNeg - raw) / (float)rangeNeg);
    }

    // Clamp to [-1, +1] to guard against ADC noise at extremes.
    if (usable > 1.0f) usable = 1.0f;
    if (usable < -1.0f) usable = -1.0f;
    return usable;
}

// ---------------------------------------------------------------------------
// Classify normalized joystick vector into a readable direction label.
// ---------------------------------------------------------------------------
inline const char* joystickDirectionLabel(float x, float y) {
    const float primary = 0.55f;
    const float secondary = 0.25f;
    const float ax = (x >= 0.0f) ? x : -x;
    const float ay = (y >= 0.0f) ? y : -y;

    if (x == 0.0f && y == 0.0f) return "CENTER";

    if (y >= primary && ax <= secondary) return "UP";
    if (y <= -primary && ax <= secondary) return "DOWN";
    if (x >= primary && ay <= secondary) return "RIGHT";
    if (x <= -primary && ay <= secondary) return "LEFT";

    if (x >= secondary && y >= secondary) return "UP-RIGHT";
    if (x <= -secondary && y >= secondary) return "UP-LEFT";
    if (x >= secondary && y <= -secondary) return "DOWN-RIGHT";
    if (x <= -secondary && y <= -secondary) return "DOWN-LEFT";

    if (ay >= ax) return (y > 0.0f) ? "UP-ish" : "DOWN-ish";
    return (x > 0.0f) ? "RIGHT-ish" : "LEFT-ish";
}

// ---------------------------------------------------------------------------
// Apply a previously computed full-range calibration (e.g. loaded from NVS).
// Call after joystickInit(); values outside the valid ADC range are ignored.
// ---------------------------------------------------------------------------
inline bool joystickApplyFullRangeCalibration(int xMin, int xMax, int yMin, int yMax) {
#ifdef JOYSTICK_FWD_BWD_ONLY
    // Dead axis measured no center; take the stored range midpoint instead
    _jsXCenter = (xMin + xMax) / 2;
#endif
    // Sanity: each range must contain the center with usable travel on both sides.
    bool xOk = (xMin < _jsXCenter - JOYSTICK_MIN_HALF_SPAN) && (xMax > _jsXCenter + JOYSTICK_MIN_HALF_SPAN);
    bool yOk = (yMin < _jsYCenter - JOYSTICK_MIN_HALF_SPAN) && (yMax > _jsYCenter + JOYSTICK_MIN_HALF_SPAN);
    if (!xOk || !yOk) {
        Logger::instance().logForced(LogLevel::WARN, TAG_JOYSTICK, __FILE__, __LINE__,
            "Full-range cal rejected (bad range): X=%d..%d Y=%d..%d centers=(%d,%d)",
            xMin, xMax, yMin, yMax, _jsXCenter, _jsYCenter);
        return false;
    }
    _jsXMin = xMin; _jsXMax = xMax;
    _jsYMin = yMin; _jsYMax = yMax;
    _jsRangeCalibrated = true;
    Logger::instance().logForced(LogLevel::INFO, TAG_JOYSTICK, __FILE__, __LINE__,
        "Full-range cal applied: X=%d..%d Y=%d..%d", xMin, xMax, yMin, yMax);
    return true;
}

// ---------------------------------------------------------------------------
// Per-direction sampling helpers for step-by-step full-range calibration.
// Block for durationMs and return the extreme raw value seen on that axis.
// The caller (serial command handler) orchestrates the directions and prints
// the instructions; these just do the ADC sampling.
// ---------------------------------------------------------------------------
// Physical pins, min/max over the window
inline void joystickSampleExtremes(unsigned long durationMs, JoystickRaw& lo, JoystickRaw& hi) {
    lo = hi = joystickReadPins();
    unsigned long end = millis() + durationMs;
    while (millis() < end) {
        JoystickRaw r = joystickReadPins();
        if (r.x < lo.x) lo.x = r.x;
        if (r.x > hi.x) hi.x = r.x;
        if (r.y < lo.y) lo.y = r.y;
        if (r.y > hi.y) hi.y = r.y;
        delay(10);
    }
}

// ---------------------------------------------------------------------------
// Main read function - returns normalized joystick state
// ---------------------------------------------------------------------------
inline JoystickNorm joystickNormalize(const JoystickRaw& raw) {
    JoystickNorm n;
    n.x = joystickNormalizeAxis(raw.x, _jsXCenter, _jsXMin, _jsXMax);
    n.y = joystickNormalizeAxis(raw.y, _jsYCenter, _jsYMin, _jsYMax);
    if (_jsInvertX) n.x = -n.x;
    if (_jsInvertY) n.y = -n.y;
    n.inDeadzone = (n.x == 0.0f && n.y == 0.0f);
    return n;
}

inline JoystickNorm joystickRead() {
    return joystickNormalize(joystickReadRaw());
}

inline int joystickExpectedCenterX() {
    return _jsRangeCalibrated ? (_jsXMin + _jsXMax) / 2 : JOYSTICK_CENTER;
}
inline int joystickExpectedCenterY() {
    return _jsRangeCalibrated ? (_jsYMin + _jsYMax) / 2 : JOYSTICK_CENTER;
}

// No reference before 'cal full' (output span may be far from 0-3.3 V); arming is locked instead
inline bool joystickRawNearExpectedCenter(const JoystickRaw& raw) {
    if (!_jsRangeCalibrated) return true;
    return abs(raw.x - joystickExpectedCenterX()) <= JOYSTICK_CENTER_TOLERANCE
        && abs(raw.y - joystickExpectedCenterY()) <= JOYSTICK_CENTER_TOLERANCE;
}

// ---------------------------------------------------------------------------
// Re-calibrate center position at runtime (joystick must be at rest).
// Lighter than joystickInit(): skips pin setup, warm-up reads, boot probe,
// and pin-map log — just re-samples the center position.
// Returns false and keeps the expected center if the stick was not at rest.
// ---------------------------------------------------------------------------
inline bool joystickRecalibrate() {
    const int calSamples = 32;
    long sumX = 0, sumY = 0;
    for (int i = 0; i < calSamples; i++) {
        sumX += analogRead(JOYSTICK_X_PIN);
        sumY += analogRead(JOYSTICK_Y_PIN);
        delay(4);
    }
    JoystickRaw sampled = { (int)(sumX / calSamples), (int)(sumY / calSamples) };
    if (_jsSwapXY) sampled = { sampled.y, sampled.x };
#ifdef JOYSTICK_FWD_BWD_ONLY
    sampled.x = joystickExpectedCenterX();
#endif
    if (!joystickRawNearExpectedCenter(sampled)) {
        _jsXCenter = joystickExpectedCenterX();
        _jsYCenter = joystickExpectedCenterY();
        Logger::instance().logForced(LogLevel::WARN, TAG_JOYSTICK, __FILE__, __LINE__,
            "Recal rejected: sample X=%d Y=%d too far from expected, using X=%d Y=%d (stick not at rest?)",
            sampled.x, sampled.y, _jsXCenter, _jsYCenter);
        return false;
    }
    _jsXCenter = sampled.x;
    _jsYCenter = sampled.y;
    Logger::instance().logForced(LogLevel::INFO, TAG_JOYSTICK, __FILE__, __LINE__,
        "Recalibrated center: X=%d Y=%d", _jsXCenter, _jsYCenter);
    return true;
}

#endif // !NO_JOYSTICK
#endif // JOYSTICK_H
