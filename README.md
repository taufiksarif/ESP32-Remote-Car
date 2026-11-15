# Tamiya Car: ESP32 craft-stick buggy

A four-wheel-drive buggy controlled from a phone browser over a Wi-Fi network hosted by the ESP32. The firmware and web interface are contained in [`TamiyaCar.ino`](TamiyaCar.ino); no separate web server or phone app is needed.

## Project parts

- DOIT ESP32 DevKit V1 with ESP32-WROOM-32 (classic ESP32)
- Four BO gear motors and matching wheels
- Two TB6612FNG dual H-bridge driver boards (four channels total, one motor per channel)
- Motor battery suitable for the BO motors and driver boards
- Craft sticks, glue, and motor mounting material (small brackets or zip ties)
- Jumper wires

The pin map is intended for the classic ESP32-WROOM-32 DevKit. It uses GPIO 2 for the onboard status LED and GPIO 4, 13, 16–19, 21–23, 25–27, and 32–33 for the motor drivers. GPIOs 34–39 are not used. If you add other hardware, check that it does not reuse these GPIOs. GPIO 16 and 17 are available on WROOM-32 modules; some other ESP32 variants reserve them for PSRAM.

## Driver and motor wiring

Use one TB6612FNG module for the two left wheels and the other for the two right wheels. On each TB6612, channel A is `AIN1`, `AIN2`, `PWMA`, `A01`, `A02`; channel B is `BIN1`, `BIN2`, `PWMB`, `B01`, `B02` (some boards print `AO1/AO2` and `BO1/BO2`). Connect ESP32 signals as follows:

| Wheel / function | Driver module pin | ESP32 GPIO |
|---|---|---:|
| Left front motor direction 1 | Left `AIN1` | 16 |
| Left front motor direction 2 | Left `AIN2` | 17 |
| Left front motor speed | Left `PWMA` | 18 |
| Left rear motor direction 1 | Left `BIN1` | 19 |
| Left rear motor direction 2 | Left `BIN2` | 21 |
| Left rear motor speed | Left `PWMB` | 22 |
| Left board enable | Left `STBY` | 4 |
| Right front motor direction 1 | Right `AIN1` | 25 |
| Right front motor direction 2 | Right `AIN2` | 26 |
| Right front motor speed | Right `PWMA` | 27 |
| Right rear motor direction 1 | Right `BIN1` | 32 |
| Right rear motor direction 2 | Right `BIN2` | 33 |
| Right rear motor speed | Right `PWMB` | 23 |
| Right board enable | Right `STBY` | 13 |

Connect each motor to its channel's output pair:

| Motor | TB6612 output pair |
|---|---|
| Left front | Left board `A01` and `A02` |
| Left rear | Left board `B01` and `B02` |
| Right front | Right board `A01` and `A02` |
| Right rear | Right board `B01` and `B02` |

For **both** driver boards, connect `VCC` (logic supply) to ESP32 `3V3`, and connect `GND` to ESP32 `GND`. Connect the motor battery positive to `VM` on both boards, and battery negative to driver `GND`. The ESP32, both drivers, and motor battery must share a common ground. Keep the ESP32 powered by USB while setting up. Never connect the motor battery to the ESP32 `3V3` pin, and do not power the motors from the ESP32.

The Wi-Fi status indicator uses the onboard LED on GPIO 2, so no additional LED or resistor is needed. It blinks while no device is connected to the ESP32 access point and stays steadily on while at least one Wi-Fi client is connected. GPIO 2 is also an ESP32 boot-strapping pin; the sketch only drives it after startup. Onboard LED wiring can vary between DevKit revisions, so the LED may be absent or active-low on some boards; if it behaves oppositely, invert the `HIGH` and `LOW` writes in the LED section of the sketch.

Choose the motor battery voltage for the specific BO motors you have, and confirm that each motor's stall current is within the TB6612FNG rating. If the motor supply is too high or the motors draw too much current, the drivers or motors can overheat.

## Assemble the craft-stick chassis

Make a low ladder frame from two long craft sticks used as side rails and three or four crosspieces. Add doubled-stick pads at the four corners, then mount one BO motor at each corner with the output shafts parallel. Secure motors with small brackets or zip ties; do not rely on glue alone to hold a motor under load.

Place the battery low and near the center between the rails. Put the ESP32 and the two driver boards on a raised center deck where they cannot touch the ground or wheels. Route wires along the rails, away from the wheels and motor shafts. A short craft-stick bumper at the front gives it a simple buggy shape. Reinforce joints with overlapping craft-stick pieces and allow glue to cure before adding batteries.

The supplied `BO-gear-motor.jpg` shows the motor and wheel set. `Craft-stick.jpg` shows the wide sticks suited to the rails and braces.

## Upload the firmware

1. Install Arduino IDE and select an ESP32 board package that provides **Arduino-ESP32 core 3.x**. The sketch uses `analogWrite()` and the built-in `WiFi` and `WebServer` libraries.
2. Open `TamiyaCar.ino`. If Arduino IDE asks to place the sketch in a same-named folder, allow it or make a folder named `TamiyaCar` and put the `.ino` file inside.
3. Select the ESP32 Dev Module (or the matching DOIT ESP32 DevKit V1 profile) and the correct serial port.
4. Upload the sketch. Open Serial Monitor at 115200 baud to see the access-point name and IP address.

This sketch does not create its own FreeRTOS tasks. The Arduino ESP32 core runs on FreeRTOS internally, while this program handles the web server in `loop()`.

## Connect and drive

1. On the phone, join Wi-Fi **Tamiya Car** with password **12345678**.
2. Open **http://10.10.10.10** in a browser. The ESP32 access point is configured to use `10.10.10.10` with subnet mask `255.255.255.0`.
3. Rotate the phone to landscape for the intended control layout.
4. Press and hold **Forward**, **Reverse**, **Left**, or **Right**. Releasing a direction or pressing **Stop** smoothly ramps the motors down. The speed slider controls the target motor PWM from 25% to 100%.

Forward and reverse drive all four wheels in the same direction. The firmware ramps motor speed up and down to reduce abrupt starts and stops. The **Turn Strength** slider controls the inner wheels during a turn: **0%** drives both sides equally (straight), low settings make a wide arc, **50%** stops the inner side, and **100%** gives the original pivot turn with one side reversing. If the browser stops sending commands for 700 ms, the ESP32 disables the drivers immediately as a failsafe.

## First power-on and direction adjustment

Raise the car so all wheels are clear of the floor before testing. Turn it on and test forward briefly. All four wheels should propel the car in the same physical forward direction. If a wheel spins the opposite way, either swap that motor's two output wires or change its polarity constant near the top of `TamiyaCar.ino` from `1` to `-1` (or vice versa). The right front and right rear polarity values are initially `-1` to account for mirrored mounting; adjust to match your actual installation.

After verifying forward and reverse, test turns with the wheels still raised, then place the buggy on a clear, flat surface. If the ESP32 resets when motors start, check the motor supply, driver wiring, and shared ground; motor noise or a sagging supply can cause resets.
