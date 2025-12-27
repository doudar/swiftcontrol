# KICKR BIKE BLE Service Implementation

## Overview

This implementation provides a **fully independent** KICKR BIKE compatible BLE service that enables virtual shifting functionality for SmartSpin2k. It emulates the Wahoo KICKR BIKE protocol and operates completely independently from FTMS (Fitness Machine Service).

## Features

- **24-Gear Virtual Shifting System**: Emulates a bicycle gear system with 24 gears ranging from very easy (0.50 ratio) to very hard (1.65 ratio)
- **Independent Gradient Control**: Directly controls trainer resistance without relying on FTMS
- **RideOn Handshake Protocol**: Full implementation of Zwift's connection handshake
- **Keep-Alive Messages**: Maintains connection with Zwift via periodic heartbeat messages
- **Shifter Position Monitoring**: Monitors `rtConfig->getShifterPosition()` for gear change inputs
- **Fully Self-Contained**: Does not require or interfere with FTMS service

## Architecture

### Service Structure

The service implements three BLE characteristics according to the KICKR BIKE protocol:

1. **Sync RX** (UUID: `00000003-19CA-4651-86E5-FA29DCDD09D1`)
   - Write characteristic for receiving commands
   - Used for future protocol extensions

2. **Async TX** (UUID: `00000002-19CA-4651-86E5-FA29DCDD09D1`)
   - Notify characteristic for asynchronous events
   - Sends gear change notifications to connected clients

3. **Sync TX** (UUID: `00000004-19CA-4651-86E5-FA29DCDD09D1`)
   - Notify characteristic for synchronous responses
   - Used for protocol handshakes and responses

### Gear System

The gear system uses a 24-gear ratio table:

```cpp
Gear  1-8:   0.50 - 0.85  (Easy - reduces effective gradient)
Gear  9-16:  0.90 - 1.25  (Medium - near neutral)
Gear 17-24:  1.30 - 1.65  (Hard - increases effective gradient)

Default: Gear 12 (ratio 1.05, slightly harder than neutral)
```

### How It Works

1. **Base Gradient**: Zwift/app sets a gradient via FTMS (e.g., 5%)
2. **Gear Ratio Applied**: Current gear ratio is multiplied with base gradient
3. **Effective Gradient**: Result is sent to the trainer

**Example:**
- Base gradient from Zwift: 5%
- Current gear: 15 (ratio 1.20)
- Effective gradient: 5% × 1.20 = 6%
- Rider feels: 20% harder than base gradient

## Usage

### Enabling/Disabling the Service

The KickrBike service can be enabled or disabled independently of FTMS:

```cpp
// Enable KickrBike service (disables FTMS gradient control)
kickrBikeService.enable();

// Disable KickrBike service (allows FTMS to control gradient)
kickrBikeService.disable();

// Check if enabled
if (kickrBikeService.isServiceEnabled()) {
  // Service is controlling the trainer
}
```

### Integration in Main Loop

To use the shifter position monitoring and keep-alive, call the update function regularly in your main loop:

```cpp
void loop() {
  // ... other code ...
  
  // Update gear based on shifter position changes
  kickrBikeService.updateGearFromShifterPosition();
  
  // ... other code ...
}
```

### Setting Gradient from External Source

When Zwift or another app sends gradient data (via TCP/mDNS protocol or other means):

```cpp
// Set base gradient from Zwift (in percentage, e.g., 5.0 for 5%)
kickrBikeService.setBaseGradient(5.0);

// The service automatically applies the current gear ratio and updates the trainer
```

### Manual Gear Changes

You can also control gears programmatically:

```cpp
// Shift to a harder gear
kickrBikeService.shiftUp();

// Shift to an easier gear
kickrBikeService.shiftDown();

// Get current gear (0-indexed, so add 1 for display)
int gear = kickrBikeService.getCurrentGear();
Serial.printf("Current gear: %d\n", gear + 1);

// Get current gear ratio
double ratio = kickrBikeService.getCurrentGearRatio();
Serial.printf("Gear ratio: %.2f\n", ratio);

// Get effective gradient (base × gear ratio)
double effectiveGrad = kickrBikeService.getEffectiveGradient();
Serial.printf("Effective gradient: %.2f%%\n", effectiveGrad);
```

### Shifter Position Integration

The service monitors `rtConfig->getShifterPosition()` for changes:

- **Position Increase**: Shifts to a harder gear (shiftUp)
- **Position Decrease**: Shifts to an easier gear (shiftDown)

**Note**: The shifter position value can be controlled via:
- Physical shifter buttons/paddles on your hardware
- BLE custom characteristic (0x17 - shifterPosition)
- Any other input mechanism that updates rtConfig->setShifterPosition()

## Configuration

### Customizing Gear Ratios

To modify the gear ratios, edit the `gearRatios` array in `BLE_KickrBikeService.cpp`:

```cpp
const double BLE_KickrBikeService::gearRatios[KICKR_BIKE_NUM_GEARS] = {
    0.50, 0.55, 0.60, 0.65, 0.70, 0.75, 0.80, 0.85,  // Easy gears
    0.90, 0.95, 1.00, 1.05, 1.10, 1.15, 1.20, 1.25,  // Medium gears
    1.30, 1.35, 1.40, 1.45, 1.50, 1.55, 1.60, 1.65   // Hard gears
};
```

### Changing Number of Gears

To use a different number of gears, modify `BLE_KickrBikeService.h`:

```cpp
#define KICKR_BIKE_NUM_GEARS 18  // For 18 gears instead of 24
#define KICKR_BIKE_DEFAULT_GEAR 8  // Middle gear (0-indexed)
```

Then update the `gearRatios` array accordingly.

## Example Scenarios

### Scenario 1: Climbing a 10% Hill

```
Zwift sends: 10% gradient
Gear 8 (0.85):  10% × 0.85 = 8.5%   (Feels easier, can maintain higher cadence)
Gear 12 (1.05): 10% × 1.05 = 10.5%  (Feels slightly harder)
Gear 18 (1.35): 10% × 1.35 = 13.5%  (Feels much harder, like climbing in big gear)
```

### Scenario 2: Flat Road (0% Gradient)

```
Zwift sends: 0% gradient
Gear 8 (0.85):  0% × 0.85 = 0%   (No effect on flat)
Gear 18 (1.35): 0% × 1.35 = 0%   (No effect on flat)

Note: On flats, gears have minimal effect unless there's some base resistance
```

### Scenario 3: Descending (-5% Gradient)

```
Zwift sends: -5% gradient
Gear 8 (0.85):  -5% × 0.85 = -4.25%  (Less descent assist)
Gear 18 (1.35): -5% × 1.35 = -6.75%  (More descent assist)
```

## Troubleshooting

### Gears Not Changing

1. Check if `updateGearFromShifterPosition()` is being called in your main loop
2. Verify `rtConfig->getShifterPosition()` is updating correctly
3. Check serial logs for "Shifted UP/DOWN" messages

### Incline Not Updating

1. Verify FTMS service is receiving gradient updates from Zwift
2. Check that `kickrBikeService.setBaseFTMSIncline()` is being called
3. Look for "KICKR BIKE: base=..." log messages

### Excessive/Insufficient Gradient Changes

1. Adjust gear ratios in the `gearRatios` array
2. For more subtle changes, use ratios closer to 1.0
3. For more dramatic changes, use ratios further from 1.0

## Advanced Usage

### Custom Gradient Calculation

You can modify the `calculateEffectiveGrade()` function for non-linear effects:

```cpp
double BLE_KickrBikeService::calculateEffectiveGrade(double baseGrade, double gearRatio) {
  // Progressive scaling: higher gears have more impact
  double progressiveFactor = pow(gearRatio, 1.5);
  return baseGrade * progressiveFactor;
}
```

### ERG Mode Considerations

In ERG mode (power target), gears can affect the "feel":
- Lower gear: Same power, but easier to maintain cadence
- Higher gear: Same power, but requires more force per pedal stroke

The current implementation focuses on SIM mode (gradient simulation).

## API Reference

### Class: BLE_KickrBikeService

#### Public Methods

- `void setupService(NimBLEServer *pServer, MyCharacteristicCallbacks *chrCallbacks)`
  - Initializes the BLE service and characteristics
  - Sets up RideOn handshake handling
  - Called automatically during BLE server setup

- `void update()`
  - Sends keep-alive messages every 5 seconds after handshake
  - Called automatically in BLE update loop

- `void enable()` / `void disable()`
  - Enable or disable the service
  - When enabled, the service controls trainer gradient
  - When disabled, FTMS can control gradient

- `bool isServiceEnabled() const`
  - Returns whether the service is currently enabled

- `void shiftUp()`
  - Shifts to next harder gear
  - Does nothing if already at highest gear
  - Automatically updates trainer gradient

- `void shiftDown()`
  - Shifts to next easier gear
  - Does nothing if already at lowest gear
  - Automatically updates trainer gradient

- `int getCurrentGear() const`
  - Returns current gear (0-indexed)
  - Range: 0 to KICKR_BIKE_NUM_GEARS-1

- `double getCurrentGearRatio() const`
  - Returns current gear ratio
  - Used for gradient calculations

- `void setBaseGradient(double gradientPercent)`
  - Sets base gradient from external source (e.g., Zwift)
  - Gradient in percentage (e.g., 5.0 for 5%)
  - Automatically applies gear ratio and updates trainer

- `double getBaseGradient() const`
  - Returns the base gradient before gear ratio applied

- `double getEffectiveGradient() const`
  - Returns effective gradient (base × gear ratio)

- `void setTargetPower(int watts)`
  - Sets target power for ERG mode
  - Power in watts

- `int getTargetPower() const`
  - Returns current target power

- `void updateGearFromShifterPosition()`
  - Checks shifter position for changes
  - Automatically shifts gears based on position delta
  - **Call this in your main loop**

- `void processWrite(const std::string& value)`
  - Handles writes to Sync RX characteristic
  - Processes RideOn handshake
  - Called automatically by BLE callbacks

- `void sendRideOnResponse()`
  - Sends RideOn handshake response
  - Called automatically after receiving RideOn

- `void sendKeepAlive()`
  - Sends keep-alive message to maintain connection
  - Called automatically every 5 seconds

## Files Modified/Added

### New Files
- `SmartSpin2k_Files/BLE_KickrBikeService.h` - Header file
- `SmartSpin2k_Files/BLE_KickrBikeService.cpp` - Implementation

### Modified Files
- `SmartSpin2k_Files/Constants.h` - Added Zwift Ride service UUIDs
- `SmartSpin2k_Files/BLE_Server.cpp` - Added service initialization and updates
- `SmartSpin2k_Files/BLE_Fitness_Machine_Service.cpp` - Added integration hooks

## Future Enhancements

Potential improvements for future versions:

1. **Protobuf Integration**: Full protocol support for Zwift Ride controller messages
2. **Battery Status**: Report battery level to connected clients
3. **Button Mapping**: Support for additional buttons (navigation, action buttons)
4. **TCP/mDNS**: Network-based protocol support (see kicker-bike-api docs)
5. **Gear Display**: Send gear number to connected displays
6. **Cadence-based Shifting**: Auto-shift based on cadence ranges

## References

- KICKR BIKE Protocol Documentation: `/kicker-bike-api/`
- Zwift Hardware API: `/zwift-hardware-api-docs/`
- FTMS Specification: Bluetooth SIG Fitness Machine Service 1.0
- BikeControl Reference: Flutter implementation of KICKR BIKE protocol

## License

Copyright (C) 2020  Anthony Doud & Joel Baranick
All rights reserved

SPDX-License-Identifier: GPL-2.0-only
