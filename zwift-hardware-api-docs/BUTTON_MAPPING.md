# Button Mapping - Actions and Configuration

## Overview

Zwift devices have physical buttons that can be mapped to various in-game actions. This document explains the button mapping system used in the BikeControl implementation.

## Button Representation

### ControllerButton Class

```dart
class ControllerButton {
  final String name;          // Unique identifier
  final InGameAction? action; // Associated action
  final IconData? icon;       // Visual icon
  final Color? color;         // Button color (for UI)
  
  const ControllerButton(
    this.name, {
    this.action,
    this.icon,
    this.color,
  });
}
```

### InGameAction Enumeration

Actions that can be triggered by buttons:

```dart
enum InGameAction {
  // Gear shifting
  shiftUp,        // Shift to harder gear
  shiftDown,      // Shift to easier gear
  
  // Navigation
  steerLeft,      // Steer left
  steerRight,     // Steer right
  uturn,          // Make U-turn
  
  // UI Control
  toggleUi,       // Show/hide UI
  openActionBar,  // Open action menu
  select,         // Confirm selection
  back,           // Go back / cancel
  
  // Power-ups and actions
  usePowerUp,     // Activate power-up
  rideOnBomb,     // Send ride-on
  
  // Additional actions
  braking,        // Brake (some apps)
  screenshot,     // Take screenshot
  // ... more actions
}
```

## Zwift Click Buttons

### Button Definitions

```dart
class ZwiftButtons {
  // Zwift Click buttons
  static const ControllerButton shiftUpRight = ControllerButton(
    'shiftUpRight',
    action: InGameAction.shiftUp,
    icon: Icons.add,
    color: Colors.black,
  );
  
  static const ControllerButton shiftUpLeft = ControllerButton(
    'shiftUpLeft',
    action: InGameAction.shiftDown,
    icon: Icons.remove,
    color: Colors.black,
  );
}
```

### Default Mapping

| Physical Button | Button Name | Default Action |
|----------------|-------------|----------------|
| Plus (+) | `shiftUpRight` | Shift Up |
| Minus (-) | `shiftUpLeft` | Shift Down |

## Zwift Play Buttons

### Left Controller

```dart
// Navigation D-pad
static const ControllerButton navigationUp = ControllerButton(
  'navigationUp',
  action: InGameAction.toggleUi,
  icon: Icons.keyboard_arrow_up,
  color: Colors.black,
);

static const ControllerButton navigationDown = ControllerButton(
  'navigationDown',
  action: InGameAction.uturn,
  icon: Icons.keyboard_arrow_down,
  color: Colors.black,
);

static const ControllerButton navigationLeft = ControllerButton(
  'navigationLeft',
  action: InGameAction.steerLeft,
  icon: Icons.keyboard_arrow_left,
  color: Colors.black,
);

static const ControllerButton navigationRight = ControllerButton(
  'navigationRight',
  action: InGameAction.steerRight,
  icon: Icons.keyboard_arrow_right,
  color: Colors.black,
);

// Control buttons
static const ControllerButton onOffLeft = ControllerButton(
  'onOffLeft',
  action: InGameAction.toggleUi,
);

static const ControllerButton sideButtonLeft = ControllerButton(
  'sideButtonLeft',
  action: InGameAction.shiftDown,
);

static const ControllerButton paddleLeft = ControllerButton(
  'paddleLeft',
  action: InGameAction.shiftDown,
);
```

### Right Controller

```dart
// Action buttons
static const ControllerButton a = ControllerButton(
  'a',
  action: InGameAction.select,
  color: Colors.lightGreen,
);

static const ControllerButton b = ControllerButton(
  'b',
  action: InGameAction.back,
  color: Colors.pinkAccent,
);

static const ControllerButton z = ControllerButton(
  'z',
  action: InGameAction.rideOnBomb,
  color: Colors.deepOrangeAccent,
);

static const ControllerButton y = ControllerButton(
  'y',
  action: null,  // No default action
  color: Colors.lightBlue,
);

// Control buttons
static const ControllerButton onOffRight = ControllerButton(
  'onOffRight',
  action: InGameAction.toggleUi,
);

static const ControllerButton sideButtonRight = ControllerButton(
  'sideButtonRight',
  action: InGameAction.shiftUp,
);

static const ControllerButton paddleRight = ControllerButton(
  'paddleRight',
  action: InGameAction.shiftUp,
);
```

### Default Mapping

**Navigation Mode** (rightPad OFF):

| Physical Button | Button Name | Default Action |
|----------------|-------------|----------------|
| D-pad Up | `navigationUp` | Toggle UI |
| D-pad Down | `navigationDown` | U-Turn |
| D-pad Left | `navigationLeft` | Steer Left |
| D-pad Right | `navigationRight` | Steer Right |
| Power Button | `onOffLeft` | Toggle UI |
| Side Button | `sideButtonLeft` | Shift Down |
| Paddle | `paddleLeft` | Shift Down |

**Action Mode** (rightPad ON):

| Physical Button | Button Name | Default Action |
|----------------|-------------|----------------|
| Y (Up) | `y` | None (customizable) |
| Z (Left) | `z` | Ride On / Elbow Flick |
| A (Right) | `a` | Select / Confirm |
| B (Down) | `b` | Back / Cancel |
| Power Button | `onOffRight` | Toggle UI |
| Side Button | `sideButtonRight` | Shift Up |
| Paddle | `paddleRight` | Shift Up |

## Zwift Ride Buttons

### All Available Buttons

```dart
// Navigation
navigationLeft, navigationRight, navigationUp, navigationDown

// Action buttons
a, b, y, z

// Left shifter
shiftUpLeft, shiftDownLeft

// Right shifter  
shiftUpRight, shiftDownRight

// Power-up buttons
powerUpLeft, powerUpRight

// Power buttons
onOffLeft, onOffRight

// Analog paddles
paddleLeft, paddleRight
```

### Shifter Buttons

```dart
// Left shifter buttons
static const ControllerButton shiftUpLeft = ControllerButton(
  'shiftUpLeft',
  action: InGameAction.shiftDown,
  icon: Icons.remove,
  color: Colors.black,
);

static const ControllerButton shiftDownLeft = ControllerButton(
  'shiftDownLeft',
  action: InGameAction.shiftDown,
);

// Right shifter buttons
static const ControllerButton shiftUpRight = ControllerButton(
  'shiftUpRight',
  action: InGameAction.shiftUp,
  icon: Icons.add,
  color: Colors.black,
);

static const ControllerButton shiftDownRight = ControllerButton(
  'shiftDownRight',
  action: InGameAction.shiftUp,
);

// Power-up buttons
static const ControllerButton powerUpLeft = ControllerButton(
  'powerUpLeft',
  action: InGameAction.shiftDown,
);

static const ControllerButton powerUpRight = ControllerButton(
  'powerUpRight',
  action: InGameAction.shiftUp,
);
```

### Default Mapping

| Physical Button | Button Name | Default Action |
|----------------|-------------|----------------|
| D-pad Up | `navigationUp` | Toggle UI |
| D-pad Down | `navigationDown` | U-Turn |
| D-pad Left | `navigationLeft` | Steer Left |
| D-pad Right | `navigationRight` | Steer Right |
| A Button | `a` | Select |
| B Button | `b` | Back |
| Y Button | `y` | Use Power-Up |
| Z Button | `z` | Ride On |
| Left Shifter + | `shiftUpLeft` | Shift Down |
| Left Shifter - | `shiftDownLeft` | Shift Down |
| Right Shifter + | `shiftUpRight` | Shift Up |
| Right Shifter - | `shiftDownRight` | Shift Up |
| Left Power-up | `powerUpLeft` | (varies) |
| Right Power-up | `powerUpRight` | (varies) |
| Left Power | `onOffLeft` | Toggle UI |
| Right Power | `onOffRight` | Toggle UI |
| Left Paddle | `paddleLeft` | Shift Down |
| Right Paddle | `paddleRight` | Shift Up |

## Action Mapping by App

Different apps use different key mappings. BikeControl includes preconfigured mappings:

### Zwift

```dart
class ZwiftApp extends SupportedApp {
  final defaultMapping = {
    InGameAction.shiftUp: KeyPair(key: 'ArrowUp', keyCode: 38),
    InGameAction.shiftDown: KeyPair(key: 'ArrowDown', keyCode: 40),
    InGameAction.steerLeft: KeyPair(key: 'ArrowLeft', keyCode: 37),
    InGameAction.steerRight: KeyPair(key: 'ArrowRight', keyCode: 39),
    InGameAction.uturn: KeyPair(key: 'ArrowDown', keyCode: 40),
    InGameAction.usePowerUp: KeyPair(key: 'Space', keyCode: 32),
    InGameAction.rideOnBomb: KeyPair(key: 'F', keyCode: 70),
    InGameAction.toggleUi: KeyPair(key: 'H', keyCode: 72),
    InGameAction.select: KeyPair(key: 'Space', keyCode: 32),
    InGameAction.back: KeyPair(key: 'Escape', keyCode: 27),
  };
}
```

### MyWhoosh

```dart
class MyWhooshApp extends SupportedApp {
  final defaultMapping = {
    InGameAction.shiftUp: KeyPair(key: 'PageUp'),
    InGameAction.shiftDown: KeyPair(key: 'PageDown'),
    InGameAction.steerLeft: KeyPair(key: 'A'),
    InGameAction.steerRight: KeyPair(key: 'D'),
    InGameAction.uturn: KeyPair(key: 'U'),
    InGameAction.usePowerUp: KeyPair(key: 'Space'),
    // ... more mappings
  };
}
```

### Rouvy

```dart
class RouvyApp extends SupportedApp {
  final defaultMapping = {
    InGameAction.shiftUp: KeyPair(key: 'ArrowUp'),
    InGameAction.shiftDown: KeyPair(key: 'ArrowDown'),
    // ... more mappings
  };
}
```

## Custom Button Mapping

### Remapping Buttons

Users can customize button actions:

```dart
class ButtonMapper {
  Map<String, InGameAction?> customMapping = {};
  
  void remapButton(ControllerButton button, InGameAction? newAction) {
    customMapping[button.name] = newAction;
  }
  
  InGameAction? getAction(ControllerButton button) {
    return customMapping[button.name] ?? button.action;
  }
}
```

### Example: Custom Mapping

```dart
// Remap Y button to screenshot
buttonMapper.remapButton(ZwiftButtons.y, InGameAction.screenshot);

// Disable a button
buttonMapper.remapButton(ZwiftButtons.z, null);

// Swap actions
buttonMapper.remapButton(ZwiftButtons.a, InGameAction.back);
buttonMapper.remapButton(ZwiftButtons.b, InGameAction.select);
```

## Multi-Button Actions

### Detecting Button Combinations

```dart
class ButtonCombinationDetector {
  Set<ControllerButton> pressedButtons = {};
  
  void handleButtonDown(ControllerButton button) {
    pressedButtons.add(button);
    checkCombinations();
  }
  
  void handleButtonUp(ControllerButton button) {
    pressedButtons.remove(button);
  }
  
  void checkCombinations() {
    // Check for specific combinations
    if (pressedButtons.contains(ZwiftButtons.a) && 
        pressedButtons.contains(ZwiftButtons.b)) {
      triggerAction(InGameAction.screenshot);
    }
  }
}
```

### Long Press Detection

```dart
class LongPressDetector {
  Timer? _longPressTimer;
  
  void handleButtonDown(ControllerButton button) {
    _longPressTimer = Timer(Duration(milliseconds: 500), () {
      onLongPress(button);
    });
  }
  
  void handleButtonUp(ControllerButton button) {
    _longPressTimer?.cancel();
    _longPressTimer = null;
  }
  
  void onLongPress(ControllerButton button) {
    // Trigger long-press action
    print('Long press: ${button.name}');
  }
}
```

## Button Event Flow

```
Physical Button Press
        ↓
BLE Notification
        ↓
Parse Protobuf Message
        ↓
Extract Button State
        ↓
Map to ControllerButton
        ↓
Get InGameAction
        ↓
Execute Action (keyboard/touch/network)
        ↓
Send to App
```

## Implementation Example

### Processing Button Events

```dart
class ButtonEventProcessor {
  final ButtonMapper mapper;
  final ActionExecutor executor;
  
  Future<void> processButtonPress(List<ControllerButton> buttons) async {
    for (final button in buttons) {
      // Get mapped action
      final action = mapper.getAction(button);
      
      if (action != null) {
        // Execute the action
        await executor.execute(action);
      }
    }
  }
}
```

### Action Execution

```dart
class ActionExecutor {
  Future<void> execute(InGameAction action) async {
    switch (action) {
      case InGameAction.shiftUp:
        await sendKeyPress('ArrowUp');
        break;
      case InGameAction.shiftDown:
        await sendKeyPress('ArrowDown');
        break;
      case InGameAction.select:
        await sendTouchEvent(x: 540, y: 960);
        break;
      // ... more actions
    }
  }
}
```

## Haptic Feedback

### Vibration on Shift

```dart
Future<void> handleButtonClick(List<ControllerButton> buttons) async {
  // Check if any button triggers shift action
  final hasShiftAction = buttons.any((b) => 
    b.action == InGameAction.shiftUp || 
    b.action == InGameAction.shiftDown
  );
  
  if (hasShiftAction && canVibrate && vibrationEnabled) {
    await vibrateDevice();
  }
}

Future<void> vibrateDevice() async {
  final vibrateCommand = Uint8List.fromList([
    0x12, 0x12, 0x08, 0x0A, 0x06, 0x08, 
    0x02, 0x10, 0x00, 0x18, 0x20
  ]);
  
  await UniversalBle.write(
    deviceId,
    serviceUuid,
    syncRxCharacteristic,
    vibrateCommand,
    withoutResponse: true,
  );
}
```

## Button State Management

### Tracking Button State

```dart
class ButtonStateManager {
  Map<String, bool> buttonStates = {};
  
  void updateButtonState(ControllerButton button, bool isPressed) {
    buttonStates[button.name] = isPressed;
  }
  
  bool isPressed(ControllerButton button) {
    return buttonStates[button.name] ?? false;
  }
  
  List<ControllerButton> getPressedButtons() {
    return ZwiftButtons.values
      .where((b) => isPressed(b))
      .toList();
  }
}
```

## Best Practices

1. **Provide default mappings** for common apps
2. **Allow customization** for power users
3. **Support button combinations** for advanced features
4. **Implement long-press** for alternate actions
5. **Provide visual feedback** for button presses
6. **Save user preferences** persistently
7. **Validate action compatibility** with current app
8. **Handle rapid button presses** gracefully

## UI for Button Configuration

### Button Configuration Screen

```dart
class ButtonConfigScreen extends StatelessWidget {
  final ControllerButton button;
  
  Widget build(BuildContext context) {
    return ListView(
      children: [
        ListTile(
          title: Text('Button: ${button.name}'),
          subtitle: Text('Current: ${button.action?.name ?? "None"}'),
        ),
        ...InGameAction.values.map((action) => 
          RadioListTile<InGameAction>(
            title: Text(action.name),
            value: action,
            groupValue: button.action,
            onChanged: (value) => remapButton(button, value),
          )
        ),
      ],
    );
  }
}
```

## Testing Button Mappings

```dart
void testButtonMapping() {
  final button = ZwiftButtons.a;
  final mapper = ButtonMapper();
  
  // Test default mapping
  assert(mapper.getAction(button) == InGameAction.select);
  
  // Test custom mapping
  mapper.remapButton(button, InGameAction.back);
  assert(mapper.getAction(button) == InGameAction.back);
  
  // Test disabled button
  mapper.remapButton(button, null);
  assert(mapper.getAction(button) == null);
}
```

## Next Steps

- Review [Examples](EXAMPLES.md) for complete implementation
- See [Protocol Buffers](PROTOBUF.md) for message parsing
- Check [Emulator Guide](EMULATOR.md) for testing without hardware
