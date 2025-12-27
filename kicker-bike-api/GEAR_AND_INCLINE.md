# Gear Changes and Incline Handling

## Overview

This document explains how to translate Zwift Ride controller button presses into gear changes and corresponding incline/resistance adjustments on your FTMS trainer. This is the **core integration** between the KICKR BIKE protocol and your existing FTMS implementation.

## Button to Action Mapping

### Shifter Buttons

The Zwift Ride controller has four shifter buttons:

| Button | Button Mask | Action | FTMS Mapping |
|--------|-------------|--------|--------------|
| Left Shift Up | `0x00100` | Shift down / Easier | Decrease gear, reduce resistance |
| Left Shift Down | `0x00200` | Shift down / Easier | Decrease gear, reduce resistance |
| Right Shift Up | `0x01000` | Shift up / Harder | Increase gear, add resistance |
| Right Shift Down | `0x02000` | Shift up / Harder | Increase gear, add resistance |

**Note**: The naming is from the Zwift Ride perspective. In practice:
- **Right shifters** (up/down) = Make pedaling **harder** (bigger gear)
- **Left shifters** (up/down) = Make pedaling **easier** (smaller gear)
- **Paddles** can also be mapped to shifting

### Navigation Buttons

| Button | Button Mask | Action | Typical Use |
|--------|-------------|--------|-------------|
| Up | `0x00002` | Navigation up | Menu navigation, toggle UI |
| Down | `0x00008` | Navigation down | U-turn in Zwift |
| Left | `0x00001` | Navigation left | Steer left |
| Right | `0x00004` | Navigation right | Steer right |

### Action Buttons

| Button | Button Mask | Action | Typical Use |
|--------|-------------|--------|-------------|
| A | `0x00010` | Confirm | Select, confirm |
| B | `0x00020` | Back | Cancel, back |
| Y | `0x00040` | Secondary | Power-up menu |
| Z | `0x00080` | Special | Ride-on, elbow flick |

## Gear System Implementation

### Option 1: Virtual Gear Table

Implement a gear table similar to real bicycles:

```dart
class GearSystem {
  int currentGear = 12;  // Start in middle gear (12 out of 24)
  
  // Gear ratios: smaller number = easier, larger = harder
  static const gearRatios = [
    0.50,  // Gear 1 (easiest)
    0.55,  // Gear 2
    0.60,  // Gear 3
    0.65,  // Gear 4
    0.70,  // Gear 5
    0.75,  // Gear 6
    0.80,  // Gear 7
    0.85,  // Gear 8
    0.90,  // Gear 9
    0.95,  // Gear 10
    1.00,  // Gear 11
    1.05,  // Gear 12 (default)
    1.10,  // Gear 13
    1.15,  // Gear 14
    1.20,  // Gear 15
    1.25,  // Gear 16
    1.30,  // Gear 17
    1.35,  // Gear 18
    1.40,  // Gear 19
    1.45,  // Gear 20
    1.50,  // Gear 21
    1.55,  // Gear 22
    1.60,  // Gear 23
    1.65,  // Gear 24 (hardest)
  ];
  
  void shiftUp() {
    if (currentGear < gearRatios.length - 1) {
      currentGear++;
      applyGearChange();
    }
  }
  
  void shiftDown() {
    if (currentGear > 0) {
      currentGear--;
      applyGearChange();
    }
  }
  
  double getCurrentRatio() {
    return gearRatios[currentGear];
  }
}
```

### Option 2: Percentage-Based Resistance

Simpler approach using percentage steps:

```dart
class ResistanceSystem {
  int resistanceLevel = 50;  // 0-100%
  static const int stepSize = 5;  // 5% per shift
  
  void shiftUp() {
    resistanceLevel = min(100, resistanceLevel + stepSize);
    applyResistance();
  }
  
  void shiftDown() {
    resistanceLevel = max(0, resistanceLevel - stepSize);
    applyResistance();
  }
}
```

## FTMS Integration

### SIM Mode (Simulation Mode)

In SIM mode, the trainer simulates riding on different gradients. When the user shifts gears, adjust the simulated incline:

```dart
void applyGearChange() {
  // Get current simulation parameters
  final currentGrade = getCurrentGrade();  // e.g., 2.5%
  final currentRatio = gearSystem.getCurrentRatio();  // e.g., 1.10
  
  // Calculate effective grade based on gear
  // Higher gear ratio = simulate steeper climb
  final baseGrade = currentGrade;  // Grade set by Zwift
  final gearMultiplier = currentRatio;
  
  // Apply gear effect (this is one approach)
  final effectiveGrade = baseGrade * gearMultiplier;
  
  // Update FTMS Indoor Bike Simulation Parameters
  updateFTMSSimulation(
    windSpeed: currentWindSpeed,
    grade: effectiveGrade,
    crr: currentCrr,
    cw: currentCw,
  );
}
```

### ERG Mode (Power Mode)

In ERG mode, the trainer maintains a target power. Gear changes can adjust the target power or the feel of the resistance:

```dart
void applyGearChange() {
  // Get current target power
  final targetPower = getCurrentTargetPower();  // e.g., 200W
  final currentRatio = gearSystem.getCurrentRatio();
  
  // Option 1: Adjust cadence feel (change resistance but maintain power)
  // This makes it feel harder/easier without changing power
  final targetCadence = 90;  // RPM
  final gearAdjustedCadence = targetCadence / currentRatio;
  
  updateFTMSTargetPower(
    targetPower: targetPower,
    feelCadence: gearAdjustedCadence,
  );
  
  // Option 2: In some trainers, you might adjust the power itself
  // (less common, as ERG mode is usually controlled by the app)
}
```

### Resistance Mode

In basic resistance mode:

```dart
void applyGearChange() {
  final currentRatio = gearSystem.getCurrentRatio();
  
  // Map gear ratio to resistance level (0-100%)
  // Assuming ratio range 0.5 to 1.65
  final normalizedRatio = (currentRatio - 0.5) / (1.65 - 0.5);
  final resistancePercent = (normalizedRatio * 100).round();
  
  updateFTMSResistance(resistancePercent);
}
```

## Complete Example: Shift Up in SIM Mode

### Scenario

- User is riding in Zwift at 2.5% grade
- Current gear: 12 (ratio 1.05)
- Current power: 200W @ 90 RPM
- User presses **right shift up** button

### Step-by-Step Processing

```dart
// 1. Receive button press from Zwift Ride controller
void handleButtonPress(List<String> buttons) {
  if (buttons.contains('shiftUpRight')) {
    handleShiftUp();
  }
}

// 2. Process shift up
void handleShiftUp() {
  print('Shift up pressed - current gear: ${gearSystem.currentGear}');
  
  // Update gear system
  gearSystem.shiftUp();  // 12 → 13
  
  print('New gear: ${gearSystem.currentGear}');
  print('New ratio: ${gearSystem.getCurrentRatio()}');  // 1.05 → 1.10
  
  // Apply to trainer
  applyGearToTrainer();
  
  // Optional: Provide user feedback
  showGearChangeNotification();
}

// 3. Apply gear change to FTMS trainer
void applyGearToTrainer() {
  final ratio = gearSystem.getCurrentRatio();  // 1.10
  
  // Get current Zwift-set parameters
  final zwiftGrade = 2.5;  // % from Zwift
  final windSpeed = 0.0;   // m/s
  final crr = 0.004;       // rolling resistance
  final cw = 0.51;         // wind resistance
  
  // Calculate effective grade with gear
  // Method 1: Direct multiplication
  final effectiveGrade = zwiftGrade * ratio;  // 2.5 * 1.10 = 2.75%
  
  // Method 2: Add offset (alternative approach)
  // final gearOffset = (ratio - 1.0) * 5.0;  // Scale factor
  // final effectiveGrade = zwiftGrade + gearOffset;
  
  print('Zwift grade: $zwiftGrade%, Effective grade: $effectiveGrade%');
  
  // Update FTMS Indoor Bike Simulation Parameters characteristic
  updateFTMSSimulation(
    windSpeed: windSpeed,
    grade: effectiveGrade,  // 2.75%
    crr: crr,
    cw: cw,
  );
}

// 4. Update FTMS characteristic
void updateFTMSSimulation({
  required double windSpeed,
  required double grade,
  required double crr,
  required double cw,
}) {
  // Encode per FTMS spec
  final windSpeedEncoded = (windSpeed * 1000).round();  // m/s to mm/s
  final gradeEncoded = (grade * 100).round();            // % to 0.01%
  final crrEncoded = (crr * 10000).round();              // to 0.0001
  final cwEncoded = (cw * 100).round();                  // to 0.01
  
  final data = ByteData(7);
  data.setInt16(0, windSpeedEncoded, Endian.little);     // bytes 0-1
  data.setInt16(2, gradeEncoded, Endian.little);         // bytes 2-3
  data.setUint8(4, crrEncoded);                          // byte 4
  data.setUint8(5, cwEncoded);                           // byte 5
  
  // Write to FTMS Indoor Bike Simulation Parameters characteristic
  // UUID: 0x2AD5
  writeFTMSCharacteristic(
    '00002AD5-0000-1000-8000-00805F9B34FB',
    data.buffer.asUint8List(),
  );
  
  print('Updated FTMS: Grade ${grade}%');
}
```

### Result

| Parameter | Before | After | Change |
|-----------|--------|-------|--------|
| Gear | 12 | 13 | +1 |
| Gear Ratio | 1.05 | 1.10 | +0.05 (4.8% harder) |
| Zwift Grade | 2.5% | 2.5% | (unchanged) |
| Effective Grade | 2.63% | 2.75% | +0.12% |
| Perceived Difficulty | Medium | Medium-Hard | Harder to pedal |

## Complete Example: Shift Down in ERG Mode

### Scenario

- User is in an ERG workout at 180W target
- Current gear: 15 (ratio 1.20)
- Current cadence: 85 RPM
- User presses **left shift up** button (shift down = easier)

### Processing

```dart
void handleShiftDown() {
  print('Shift down pressed - current gear: ${gearSystem.currentGear}');
  
  gearSystem.shiftDown();  // 15 → 14
  
  print('New gear: ${gearSystem.currentGear}');
  print('New ratio: ${gearSystem.getCurrentRatio()}');  // 1.20 → 1.15
  
  applyGearToERGMode();
}

void applyGearToERGMode() {
  final ratio = gearSystem.getCurrentRatio();  // 1.15
  final targetPower = 180;  // Watts (set by Zwift/workout)
  
  // In ERG mode, power is fixed. We adjust the "feel"
  // Lower gear = easier = higher suggested cadence for same power
  
  final baseCadence = 90;  // RPM
  final gearAdjustedCadence = baseCadence / ratio;  // 90 / 1.15 = 78 RPM
  
  // Update trainer to target this cadence feel
  // This makes it easier to spin at the target power
  updateERGModeFeel(
    targetPower: targetPower,
    targetCadence: gearAdjustedCadence,
  );
  
  print('ERG mode: ${targetPower}W, target cadence feel: ${gearAdjustedCadence.round()} RPM');
}
```

### Result

| Parameter | Before | After | Change |
|-----------|--------|-------|--------|
| Gear | 15 | 14 | -1 |
| Gear Ratio | 1.20 | 1.15 | -0.05 |
| Target Power | 180W | 180W | (unchanged) |
| Cadence Feel | 75 RPM | 78 RPM | Easier to spin |

## Example: Multiple Shifts

User shifts up 3 times rapidly:

```dart
// Receive button events
handleButtonPress(['shiftUpRight']);  // Gear 12 → 13
await Future.delayed(Duration(milliseconds: 200));

handleButtonPress(['shiftUpRight']);  // Gear 13 → 14
await Future.delayed(Duration(milliseconds: 200));

handleButtonPress(['shiftUpRight']);  // Gear 14 → 15

// Final state
// Gear: 15
// Ratio: 1.20 (20% harder than neutral gear 11)
// If starting at 2% grade: effective 2.4% grade
```

## Paddle Shifters

If using the analog paddles for shifting:

```dart
void handlePaddlePress(int location, int value) {
  if (location == 0) {  // Left paddle
    handleShiftDown();
  } else if (location == 1) {  // Right paddle
    handleShiftUp();
  }
}
```

## Advanced: Progressive Resistance

For more realistic feel, apply non-linear gear ratios:

```dart
double calculateEffectiveGrade(double baseGrade, double ratio) {
  // Progressive scaling: higher gears have more impact
  final progressiveFactor = pow(ratio, 1.5);
  return baseGrade * progressiveFactor;
}

// Example:
// Gear 12 (ratio 1.05): 2.5% * 1.05^1.5 = 2.5% * 1.076 = 2.69%
// Gear 18 (ratio 1.35): 2.5% * 1.35^1.5 = 2.5% * 1.570 = 3.93%
```

## Gear Change Feedback

### Visual/Haptic Feedback

```dart
void showGearChangeNotification() {
  // Option 1: Display on trainer screen
  displayMessage('Gear ${gearSystem.currentGear}');
  
  // Option 2: LED indicators
  updateGearLEDs(gearSystem.currentGear);
  
  // Option 3: Haptic feedback (if supported)
  vibrate(duration: 50);
}
```

### Audio Feedback

```dart
void playGearChangeSound() {
  if (isShiftingUp) {
    playSound('shift_up.wav');
  } else {
    playSound('shift_down.wav');
  }
}
```

## Handling Edge Cases

### Maximum Gear

```dart
void shiftUp() {
  if (currentGear >= gearRatios.length - 1) {
    print('Already in highest gear');
    playErrorSound();
    return;
  }
  currentGear++;
  applyGearChange();
}
```

### Minimum Gear

```dart
void shiftDown() {
  if (currentGear <= 0) {
    print('Already in lowest gear');
    playErrorSound();
    return;
  }
  currentGear--;
  applyGearChange();
}
```

### Grade Limits

```dart
void applyGearToTrainer() {
  var effectiveGrade = calculateEffectiveGrade(zwiftGrade, ratio);
  
  // Clamp to FTMS limits (-20% to +20%)
  effectiveGrade = effectiveGrade.clamp(-20.0, 20.0);
  
  updateFTMSSimulation(grade: effectiveGrade);
}
```

## Integration with Existing FTMS

### Preserve Zwift Control

Your gear system should work **alongside** Zwift's control, not replace it:

```dart
// When Zwift updates the grade
void onZwiftGradeChange(double newGrade) {
  baseGrade = newGrade;
  
  // Reapply current gear to new base grade
  applyGearToTrainer();
}

// When Zwift updates target power (ERG)
void onZwiftPowerChange(double newPower) {
  targetPower = newPower;
  
  // Reapply current gear feel
  applyGearToERGMode();
}
```

## Testing Your Implementation

### Test Cases

1. **Shift up from middle gear**
   - Press right shifter
   - Verify gear increases
   - Verify resistance increases

2. **Shift down to lowest gear**
   - Press left shifter repeatedly
   - Verify stops at gear 1
   - Verify doesn't go below

3. **Rapid shifting**
   - Press shifter 5 times quickly
   - Verify all shifts registered
   - Verify smooth resistance changes

4. **Grade change during shifting**
   - Shift to gear 15
   - Zwift changes grade to 5%
   - Verify effective grade = 5% * 1.25 = 6.25%

5. **ERG mode compatibility**
   - Start ERG workout
   - Shift gears
   - Verify power remains constant
   - Verify feel changes (easier/harder to spin)

## Implementation Checklist

- [ ] Implement gear system (table or percentage-based)
- [ ] Map shift up buttons to gear increase
- [ ] Map shift down buttons to gear decrease
- [ ] Calculate effective grade from gear ratio
- [ ] Update FTMS simulation parameters
- [ ] Handle SIM mode gear changes
- [ ] Handle ERG mode gear changes (if applicable)
- [ ] Implement min/max gear limits
- [ ] Clamp effective values to FTMS limits
- [ ] Preserve Zwift's base control values
- [ ] Add gear change feedback (optional)
- [ ] Test with Zwift in SIM mode
- [ ] Test with Zwift in ERG mode
- [ ] Test rapid shifting
- [ ] Test edge cases (min/max gears)

## Reference Code

See BikeControl implementation:
- File: `lib/bluetooth/devices/zwift/ftms_mdns_emulator.dart`
- Lines: 336-386 (sendAction method)
- Shows how button presses are translated to actions

## Next Steps

After implementing gear and incline handling, proceed to **[IMPLEMENTATION_GUIDE.md](IMPLEMENTATION_GUIDE.md)** for a complete step-by-step integration guide.
