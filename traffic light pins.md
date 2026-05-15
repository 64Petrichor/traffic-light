## Page 1

# Adaptive Traffic Light System
Pin Mapping & Wiring Reference | ESP32 Platform

## 1. RFID-RC522 — SPI Interface

<table>
  <thead>
    <tr>
      <th>RFID-RC522 Pin</th>
      <th>ESP32 GPIO</th>
      <th>Notes</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td>MOSI</td>
      <td>GPIO 23</td>
      <td>SPI default</td>
    </tr>
    <tr>
      <td>MISO</td>
      <td>GPIO 19</td>
      <td>SPI default</td>
    </tr>
    <tr>
      <td>SCK</td>
      <td>GPIO 18</td>
      <td>SPI default</td>
    </tr>
    <tr>
      <td>SDA (SS)</td>
      <td>GPIO 5</td>
      <td>Chip select</td>
    </tr>
    <tr>
      <td>RST</td>
      <td>GPIO 4</td>
      <td>Reset</td>
    </tr>
    <tr>
      <td>3.3V</td>
      <td>3.3V</td>
      <td>Power — do NOT use 5V</td>
    </tr>
    <tr>
      <td>GND</td>
      <td>GND</td>
      <td></td>
    </tr>
  </tbody>
</table>

△ RC522 operates at 3.3V logic — directly compatible with ESP32. Never connect to 5V.

## 2. 74HC595N Shift Register — Control Pins

<table>
  <thead>
    <tr>
      <th>74HC595N Pin</th>
      <th>ESP32 GPIO</th>
      <th>Notes</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td>DS / SER (Data)</td>
      <td>GPIO 27</td>
      <td>Serial data in</td>
    </tr>
    <tr>
      <td>SHCP / SRCLK (Clock)</td>
      <td>GPIO 26</td>
      <td>Shift clock</td>
    </tr>
    <tr>
      <td>STCP / RCLK (Latch)</td>
      <td>GPIO 25</td>
      <td>Storage clock / latch</td>
    </tr>
    <tr>
      <td>VCC</td>
      <td>3.3V</td>
      <td>Power</td>
    </tr>
    <tr>
      <td>GND</td>
      <td>GND</td>
      <td></td>
    </tr>
    <tr>
      <td>OE (Output Enable)</td>
      <td>GND</td>
      <td>Tie LOW to always enable outputs</td>
    </tr>
    <tr>
      <td>MR (Master Reset)</td>
      <td>3.3V</td>
      <td>Tie HIGH to disable reset</td>
    </tr>
  </tbody>
</table>

## 3. 74HC595N — LED Output Mapping (8 LEDs: All Reds + All Yellows)

<table>
  <thead>
    <tr>
      <th>Shift Register Output</th>
      <th>LED</th>
      <th>Road</th>
      <th>Color</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td>Q0</td>
      <td>LED 1</td>
      <td>Top</td>
      <td>Red</td>
    </tr>
    <tr>
      <td>Q1</td>
      <td>LED 2</td>
      <td>Bottom</td>
      <td>Red</td>
    </tr>
    <tr>
      <td>Q2</td>
      <td>LED 3</td>
      <td>Left</td>
      <td>Red</td>
    </tr>
    <tr>
      <td>Q3</td>
      <td>LED 4</td>
      <td>Right</td>
      <td>Red</td>
    </tr>
    <tr>
      <td>Q4</td>
      <td>LED 5</td>
      <td>Top</td>
      <td>Yellow</td>
    </tr>
  </tbody>
</table>

---


## Page 2

<table>
  <thead>
    <tr>
      <th>Shift Register Output</th>
      <th>LED</th>
      <th>Road</th>
      <th>Color</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td>Q5</td>
      <td>LED 6</td>
      <td>Bottom</td>
      <td>Yellow</td>
    </tr>
    <tr>
      <td>Q6</td>
      <td>LED 7</td>
      <td>Left</td>
      <td>Yellow</td>
    </tr>
    <tr>
      <td>Q7</td>
      <td>LED 8</td>
      <td>Right</td>
      <td>Yellow</td>
    </tr>
  </tbody>
</table>
▲ Each LED output connects through a 220Ω resistor to the LED anode. LED cathode to GND.

## 4. Direct GPIO LEDs (4 Green LEDs)

<table>
  <thead>
    <tr>
      <th>ESP32 GPIO</th>
      <th>LED</th>
      <th>Road</th>
      <th>Color</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td>GPIO 2</td>
      <td>LED 9</td>
      <td>Top</td>
      <td>Green</td>
    </tr>
    <tr>
      <td>GPIO 16</td>
      <td>LED 10</td>
      <td>Bottom</td>
      <td>Green</td>
    </tr>
    <tr>
      <td>GPIO 17</td>
      <td>LED 11</td>
      <td>Left</td>
      <td>Green</td>
    </tr>
    <tr>
      <td>GPIO 21</td>
      <td>LED 12</td>
      <td>Right</td>
      <td>Green</td>
    </tr>
  </tbody>
</table>
▲ Each GPIO connects through a 220Ω resistor to the LED anode. LED cathode to GND.
▲ GPIO 2 has boot-mode implications — add a 10KΩ pull-down resistor to GND.

## 5. Emergency Red LED

<table>
  <thead>
    <tr>
      <th>Component Pin</th>
      <th>ESP32 GPIO / Rail</th>
      <th>Notes</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td>LED anode</td>
      <td>GPIO 22</td>
      <td>Signal pin — use digitalWrite(); through 220Ω resistor</td>
    </tr>
    <tr>
      <td>LED cathode</td>
      <td>GND</td>
      <td></td>
    </tr>
  </tbody>
</table>

## 6. LDR (Light Dependent Resistor)

<table>
  <thead>
    <tr>
      <th>Connection</th>
      <th>ESP32 GPIO / Rail</th>
      <th>Notes</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td>LDR leg 1</td>
      <td>3.3V</td>
      <td></td>
    </tr>
    <tr>
      <td>LDR leg 2 + 10KΩ leg 1</td>
      <td>GPIO 34</td>
      <td>Analog read — ADC1, input-only pin</td>
    </tr>
    <tr>
      <td>10KΩ leg 2</td>
      <td>GND</td>
      <td>Pull-down resistor</td>
    </tr>
  </tbody>
</table>
▲ GPIO 34 is ADC1 — works while WiFi is active. ADC2 pins (e.g. GPIO 13, 14) do not.

## 7. Resistors Summary

<table>
  <thead>
    <tr>
      <th>Purpose</th>
      <th>Value</th>
      <th>Qty</th>
      <th>Connected Between</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td>LED current limiting (all 13 LEDs)</td>
      <td>220Ω</td>
      <td>13</td>
      <td>Each LED anode → GPIO / 595 output</td>
    </tr>
    <tr>
      <td>GPIO 2 pull-down (boot safety)</td>
      <td>10KΩ</td>
      <td>1</td>
      <td>GPIO 2 → GND</td>
    </tr>
    <tr>
      <td>LDR pull-down</td>
      <td>10KΩ</td>
      <td>1</td>
      <td>GPIO 34 → GND</td>
    </tr>
  </tbody>
</table>

## 8. Power Rails

3.3V rail → ESP32 3.3V output → RFID-RC522 VCC, 74HC595N VCC & MR
GND → Common ground across all components (ESP32, 595, RFID, LEDs)
▲ LCD removed from design. Voltage regulator and 9V supply no longer needed.

---


## Page 3

8. GPIO Usage Summary

<table>
  <thead>
    <tr>
      <th>Function</th>
      <th>GPIO Pins Used</th>
      <th>Count</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td>RFID-RC522 (SPI)</td>
      <td>GPIO 4, 5, 18, 19, 23</td>
      <td>5</td>
    </tr>
    <tr>
      <td>74HC595N Shift Register</td>
      <td>GPIO 25, 26, 27</td>
      <td>3</td>
    </tr>
    <tr>
      <td>Green LEDs (direct)</td>
      <td>GPIO 2, 16, 17, 21</td>
      <td>4</td>
    </tr>
    <tr>
      <td>Emergency Red LED</td>
      <td>GPIO 22</td>
      <td>1</td>
    </tr>
    <tr>
      <td>LDR</td>
      <td>GPIO 34</td>
      <td>1</td>
    </tr>
    <tr>
      <td>Total</td>
      <td></td>
      <td>14 / ~26 usable</td>
    </tr>
  </tbody>
</table>

Remaining free GPIOs: 0, 13, 14, 15, 32, 33 (previously used by LCD — now available)
△ GPIO 35, 36, 39 are input-only — not used in this design.
△ GPIO 0, 12, 15 have boot-mode implications — avoid using them as outputs.