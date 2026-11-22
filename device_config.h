/*
 * Device Configuration - ESP32 M25 Remote Control
 *
 * Hardware: ESP32-WROOM-32 (or ESP32-WROVER)
 *
 * Test setup: simple push buttons, single-color LEDs, no display.
 *
 * PIN RESTRICTIONS (ESP32-WROOM-32):
 *   Input-only  : 34, 35, 36 (VP), 39 (VN) - no internal pull-up/down
 *   Boot pins   : 0 (must be HIGH at boot), 2 (must be LOW/float at boot),
 *                 15 (MTDO, keep LOW at boot for normal UART download)
 *   Flash SPI   : 6-11  - never touch these
 *   ADC2        : 0, 2, 4, 12, 13, 14, 15, 25, 26, 27
 *                 ADC2 cannot be used for analogRead() while BT/WiFi radio is active.
 *                 Using them as plain digital I/O is safe.
 */

#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

 // ---------------------------------------------------------------------------
 // Encryption keys (AES-128, 16 bytes)
 // Replace with actual keys derived from wheel QR codes (m25_qr_to_key.py)
 // ---------------------------------------------------------------------------
#ifndef ENCRYPTION_KEY_LEFT
#define ENCRYPTION_KEY_LEFT  { \
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  \
}
#endif

#ifndef ENCRYPTION_KEY_RIGHT
#define ENCRYPTION_KEY_RIGHT { \
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  \
}
#endif

// M25 wheel BLE MAC addresses - default values, overridden by load_env.py build
// flags or by 'setmac' serial command (persisted in NVS).
#ifndef LEFT_WHEEL_MAC
#define LEFT_WHEEL_MAC  "28:05:a5:34:d8:9e"
#endif
#ifndef RIGHT_WHEEL_MAC
#define RIGHT_WHEEL_MAC "28:05:a5:35:7b:9a"
#endif

// ---------------------------------------------------------------------------
// Wheel operating mode
// Set WHEEL_MODE to match the hardware you have available.
//   WHEEL_MODE_DUAL        - both wheels are connected and commanded
//   WHEEL_MODE_LEFT_ONLY   - only the left  wheel (right is ignored)
//   WHEEL_MODE_RIGHT_ONLY  - only the right wheel (left  is ignored)
// All BLE and motor functions automatically skip inactive wheels.
// Change back to WHEEL_MODE_DUAL when both wheels are present.
// ---------------------------------------------------------------------------
#define WHEEL_MODE_DUAL        0
#define WHEEL_MODE_LEFT_ONLY   1
#define WHEEL_MODE_RIGHT_ONLY  2

#define WHEEL_MODE  WHEEL_MODE_DUAL   // <-- set to WHEEL_MODE_DUAL for normal operation

#if   WHEEL_MODE == WHEEL_MODE_DUAL
#define WHEEL_MODE_NAME "Dual"
#elif WHEEL_MODE == WHEEL_MODE_LEFT_ONLY
#define WHEEL_MODE_NAME "Left only"
#elif WHEEL_MODE == WHEEL_MODE_RIGHT_ONLY
#define WHEEL_MODE_NAME "Right only"
#else
#define WHEEL_MODE_NAME "Unknown"
#endif

// ---------------------------------------------------------------------------
// Transport selection
// Exactly one transport must be enabled.  Default: BLE.
// Override via PlatformIO build flag:
//   -DM25_TRANSPORT_BLE=0 -DM25_TRANSPORT_RFCOMM=1
// ---------------------------------------------------------------------------
#ifndef M25_TRANSPORT_RFCOMM
#define M25_TRANSPORT_RFCOMM 0
#endif

#ifndef M25_TRANSPORT_BLE
#define M25_TRANSPORT_BLE 1
#endif

#if (M25_TRANSPORT_RFCOMM + M25_TRANSPORT_BLE) != 1
#error "Enable exactly one transport: M25_TRANSPORT_RFCOMM or M25_TRANSPORT_BLE"
#endif

// ---------------------------------------------------------------------------
// Analog Inputs - ADC1 only (safe while Bluetooth is active)
// ---------------------------------------------------------------------------
// Joystick:  standard KY-023 or similar 10k potentiometer module
//            VCC -> 3.3 V,  GND -> GND
//            VRx and VRy output 0-3.3 V (no voltage divider needed at 3.3 V)
#define JOYSTICK_X_PIN  32   // ADC1_CH4  - joystick X-axis (left/right)
#define JOYSTICK_Y_PIN  33   // ADC1_CH5  - joystick Y-axis (forward/backward)

// Battery voltage divider: 100k (high side) + 100k (low side) -> GPIO 36
// Scales LiPo 3.0-4.2 V to 1.5-2.1 V (well within 3.3 V ADC range)
#define BATTERY_ADC_PIN 36   // ADC1_CH0 (VP) - input-only, no pull

// Set this to enable battery monitoring (ADC read, LED, auto-shutdown).
// Requires the voltage divider to be wired to BATTERY_ADC_PIN.
// #define ENABLE_BATTERY_MONITOR

// Define when no joystick is physically connected (e.g. during bench tests).
// joystickInit() becomes a no-op; joystickRead() always returns centered/
// deadzone so the system boots through the safety check without ADC noise.
// #define NO_JOYSTICK

// TEMP: left/right input is dead (reads 0). Pins it to center so the stored
// cal stays valid and the stick drives forward/backward only.
#define JOYSTICK_FWD_BWD_ONLY

#ifdef JOYSTICK_FWD_BWD_ONLY
#warning "JOYSTICK_FWD_BWD_ONLY: left/right axis ignored, no steering. Temp test build."
#endif

// ---------------------------------------------------------------------------
// Digital Inputs - active LOW, internal pull-up enabled
// ---------------------------------------------------------------------------
// Note: GPIO 14 is a bootstrap pin (MTMS). It has an internal 10k pull-up and
//       is sampled at boot; a button press at power-on is harmless as long as
//       the rest of the boot circuit is correct (no JTAG debugger attached).
#define BTN_ESTOP_PIN      14   // Emergency stop  (index finger, top edge)
#define BTN_HILL_HOLD_PIN  25   // Hill hold toggle (side button)
#define BTN_ASSIST_PIN     26   // Assist level cycle (side button)
#define BTN_POWER_PIN      13   // Power on/off toggle

// ---------------------------------------------------------------------------
// PWM LED Outputs (LEDC peripheral, 8-bit, 5 kHz)
// ---------------------------------------------------------------------------
#define LED_STATUS_PIN     16   // Red    - system status / error indicator
#define LED_BLE_PIN        17   // White  - BLE: blink slow = searching, solid = connected
#define LED_HILL_HOLD_PIN  18   // Yellow - hill hold active indicator
#define LED_ASSIST_PIN     19   // Green  - assist level indicator
#define LED_BATTERY_PIN    27   // Red    - battery charge level indicator

// ---------------------------------------------------------------------------
// Buzzer (audio feedback)
// ---------------------------------------------------------------------------
// Default: passive piezo, driven with a tone straight from the GPIO.
// Define BUZZER_ACTIVE for a buzzer with built-in oscillator (on/off drive).
// Active buzzers and magnetic (coil) passive buzzers need a transistor:
// they draw more current than a GPIO should source.
// Recommended pins: 23, 22, 21, 5. Runtime override: 'pinmap buzzer'.
#define BUZZER_PIN         23
#define BUZZER_TONE_HZ     2700  // near the resonance of common piezo discs
// #define BUZZER_ACTIVE

// LEDC channel assignments (0-7 are available in single-core mode)
#define LEDC_CH_STATUS     0
#define LEDC_CH_BATTERY    1
#define LEDC_CH_HILL_HOLD  2
#define LEDC_CH_ASSIST     3
#define LEDC_CH_BLE        4

#define LEDC_FREQ_HZ       5000  // PWM frequency
#define LEDC_RESOLUTION    8     // bits -> duty cycle 0-255

// LED ON duty (full brightness; lower for less eye strain)
#define LED_DUTY_ON        200   // 0-255
#define LED_DUTY_OFF       0

// ---------------------------------------------------------------------------
// Joystick calibration
// ---------------------------------------------------------------------------
#define ADC_RESOLUTION_BITS 12          // 12-bit ADC -> 0-4095
#define JOYSTICK_ADC_MIN    0
#define JOYSTICK_ADC_MAX    4095
#define JOYSTICK_CENTER     2048        // Ideal midpoint; calibrated at runtime
// % of travel, not counts: fixed counts make short-travel sticks feel numb
#define JOYSTICK_DEADZONE_PCT        10
#define JOYSTICK_DEADZONE_MIN_COUNTS 40   // ADC noise floor
#define JOYSTICK_MIN_HALF_SPAN       300  // shortest center-to-end travel 'cal full' accepts
// Rest center vs. 'cal full' midpoint; catches a stick held deflected at boot
#define JOYSTICK_CENTER_TOLERANCE 400
#define ADC_SAMPLES         10          // Oversampling for noise reduction

// ---------------------------------------------------------------------------
// Motor speed mapping
// Vmax values in protocol units (0-100 % of hardware maximum speed)
// ---------------------------------------------------------------------------
#define VMAX_INDOOR     60   // 6 km/h  - indoor / narrow spaces
#define VMAX_OUTDOOR    80   // 8 km/h  - outdoor / pavement
#define VMAX_LEARNING   30   // 3 km/h  - learning / slow mode

// Reverse speed is limited to this fraction of forward Vmax
#define VMAX_REVERSE_RATIO  0.50f

// Differential steering: inner wheel speed = outer * (1 - TURN_REDUCTION)
// At full X deflection the inner wheel is slowed by TURN_REDUCTION (0.0-1.0)
#define TURN_REDUCTION  0.50f     // inner wheel gets 50 % of outer speed

// ---------------------------------------------------------------------------
// Timing
// ---------------------------------------------------------------------------
#define COMMAND_RATE_MS       50    // 20 Hz motor command update
#define WATCHDOG_TIMEOUT_MS   5000  // Auto-stop if no joystick input for 5 s
#define WATCHDOG_WARN_MS      3000  // Visual warning starts at 3 s
#define DEBOUNCE_MS           50    // Button debounce window

// Set this when using a self-centering joystick (spring-loaded).
// With a potentiometer the joystick never returns to center on its own, so
// the idle watchdog would fire constantly.  Comment out for pot / bench use.
// #define ENABLE_IDLE_WATCHDOG

// Joystick state-change hysteresis
// Prevents ADC noise at the deadzone boundary from bouncing between
// READY and OPERATING.  Joystick must be outside / inside the deadzone
// continuously for these durations before a transition is accepted.
#define JS_ACTIVATE_HOLD_MS   100   // deadzone -> OPERATING  (must push for 100 ms)
#define JS_IDLE_HOLD_MS       300   // OPERATING -> deadzone -> READY (must center for 300 ms)

// ARMED state remote-mode keep-alive interval (ms).
// M25 exits remote mode if no REMOTE_SPEED arrives for ~200-500 ms; 80 ms gives
// adequate margin while the joystick is centred.
#define ARMED_KEEPALIVE_INTERVAL_MS  80

// Blink periods
#define BLINK_SLOW_MS         1000  // 1 Hz  (slow blink, e.g. learning mode)
#define BLINK_FAST_MS         500   // 2 Hz  (fast blink, e.g. error / low bat)

// ---------------------------------------------------------------------------
// Battery thresholds (as percentage 0-100)
// ---------------------------------------------------------------------------
#define BATT_WARN_LOW_PCT      30   // Slow blink warning
#define BATT_WARN_CRITICAL_PCT 15   // Fast blink + lockout for new connections
#define BATT_HALF_PCT          50   // LED turns on (solid) below this
#define BATT_AUTO_OFF_PCT      10   // Force disconnect + shutdown

// Battery ADC reference values (with 1:2 voltage divider, 3.3 V ADC ref)
// at 4.2 V  -> 2.1 V -> ADC ~ 2607   (100 %)
// at 3.0 V  -> 1.5 V -> ADC ~ 1862   (  0 %)
#define BATT_ADC_FULL   2607
#define BATT_ADC_EMPTY  1862

// ---------------------------------------------------------------------------
// Assist level definitions (used as array index, do NOT change order)
// ---------------------------------------------------------------------------
#define ASSIST_INDOOR   0
#define ASSIST_OUTDOOR  1
#define ASSIST_LEARNING 2
#define ASSIST_COUNT    3

// ---------------------------------------------------------------------------
// System state machine (legacy - kept for serial commands compatibility)
// When using Supervisor, the actual state is SupervisorState from types.h
// ---------------------------------------------------------------------------
enum SystemState : uint8_t {
    STATE_BOOT = 0,
    STATE_CONNECTING = 1,
    STATE_READY = 2,
    STATE_OPERATING = 3,
    STATE_ERROR = 4,
    STATE_OFF = 5,
};

// ---------------------------------------------------------------------------
// Arm mode
// AUTO_ARM_ON_CONNECT sets the default for arming on BLE connect; the serial
// 'autoarm on|off' command overrides it in NVS. After a disarm (idle timeout,
// assist hold) re-arm with assist hold or the stick gesture.
// ---------------------------------------------------------------------------
// #define AUTO_ARM_ON_CONNECT

// Hardware arming: hold assist, or stick gesture (see arm_gesture.h).
// Neutral hold keeps a stick held through arm/reconnect from starting a drive.
#define ARM_BUTTON_HOLD_MS          1000
#define ARM_NEUTRAL_HOLD_MS         500
#define ARM_IDLE_TIMEOUT_MS         60000
#define ARM_GESTURE_THRESHOLD       0.70f
#define ARM_GESTURE_OFF_AXIS_MAX    0.35f
#define ARM_GESTURE_FWD_HOLD_MS     1000
#define ARM_GESTURE_BACK_MIN_MS     80
#define ARM_GESTURE_BACK_MAX_MS     700
#define ARM_GESTURE_STEP_TIMEOUT_MS 1000

// ---------------------------------------------------------------------------
// Deadman switch
// Without NO_DEADMAN_HARDWARE the firmware expects a dedicated hold-to-drive
// button wired to the deadman input.  Define NO_DEADMAN_HARDWARE to tie the
// deadman permanently high - in that case the joystick leaving the deadzone
// is the only trigger required to start driving (no separate button needed).
//
// SAFETY: with NO_DEADMAN_HARDWARE a stuck joystick (hardware fault, vibration,
// ADC noise) will command the wheelchair to drive without user intent.
// Remove this define as soon as a physical deadman button is wired.
//
// Note: the mapper deadzone and the supervisor input watchdog still provide
// layered safety - releasing the joystick to centre stops the wheels immediately.
// ---------------------------------------------------------------------------
#define NO_DEADMAN_HARDWARE   // TODO: remove when deadman button is wired

#ifdef NO_DEADMAN_HARDWARE
#warning "NO_DEADMAN_HARDWARE is enabled: the deadman interlock is bypassed (joystick-only drive). Remove before use on a real chair."
#endif

#endif // DEVICE_CONFIG_H
