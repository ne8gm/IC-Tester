# IC Tester Using ATmega32A and Dual 74HC4051

## Overview

This project is an 8-pin IC testing platform built using an ATmega32A microcontroller and two 74HC4051 8-channel analog multiplexers. The system allows the user to test IC pins through a UART-based menu and supports both passive measurement and active injection-based analysis.

The firmware was developed using CodeVisionAVR and communicates with a serial terminal at 9600 baud. The project focuses on stable ADC measurements by using averaging, dummy ADC conversions after multiplexer switching, settling delays, and outlier rejection.

## Main Features

- Tests 8-pin ICs using two 74HC4051 analog multiplexers.
- Uses ATmega32A as the main controller.
- UART menu for selecting the operating mode.
- Mode 1: Single MUX scan for measurement only.
- Mode 2: Dual MUX matrix test using injection and measurement.
- Automatic ground-pin detection in Mode 2.
- ADC noise reduction using averaging and dummy reads.
- Outlier rejection by trimming the highest and lowest samples.
- Normalized display of measurement results.
- Safe startup behavior with injection disabled by default.

## Hardware Components

| Component | Quantity | Description |
|---|---:|---|
| ATmega32A | 1 | Main microcontroller |
| 74HC4051 analog multiplexer | 2 | One MUX for injection and one MUX for measurement |
| IC socket / fixture | 1 | Holds the 8-pin IC under test |
| USB-UART adapter | 1 | Used for serial communication |
| 5V supply | 1 | Powers the circuit |
| Decoupling capacitors | Recommended | 0.1 uF near the MCU and each MUX |

## Software Requirements

- CodeVisionAVR
- AVR programmer
- Serial terminal such as PuTTY or Tera Term
- UART settings: 9600 baud, 8 data bits, no parity, 1 stop bit (8N1)

## Hardware Design

The design uses two 74HC4051 multiplexers:

### MUX1 - Injection MUX

MUX1 is used to inject VCC into one selected IC pin. Its COM pin is connected to +5V, and the selected channel determines which IC pin receives the injected voltage.

### MUX2 - Measurement MUX

MUX2 is used to measure the voltage on each IC pin. Its COM pin is connected to ADC0 of the ATmega32A, allowing the firmware to read the selected pin voltage.

## Pin Mapping

| Signal | ATmega32A Pin | Connected To | Purpose |
|---|---|---|---|
| MUX1 S0-S2 | PD2-PD4 | MUX1 select pins | Select injection channel |
| MUX2 S0-S2 | PD5-PD7 | MUX2 select pins | Select measurement channel |
| MUX1 INH | PC4 | MUX1 inhibit pin | Enable or disable injection |
| ADC0 | PA0 | MUX2 COM | Read selected IC pin voltage |
| MUX1 COM | VCC | +5V | Inject VCC to selected channel |

## Operating Modes

### Mode 1 - Single MUX Scan

Mode 1 performs a passive scan using only the measurement multiplexer. MUX1 is disabled, so no voltage is injected into the IC pins.

This mode scans all 8 channels through MUX2 and displays the raw ADC value and the converted voltage in millivolts.

Key parameters:

| Parameter | Value | Description |
|---|---:|---|
| SETTLE_MS_SINGLE | 10 ms | Delay after switching MUX2 channel |
| SAMPLE_DELAY_MS_SINGLE | 2 ms | Delay between ADC samples |
| AVG_SAMPLES_SINGLE | 20 | Number of samples averaged per channel |
| VCC_MV | 5000 mV | Used for ADC-to-millivolt conversion |

### Mode 2 - Dual MUX Matrix Test

Mode 2 performs an active test using both multiplexers. MUX1 injects VCC into one IC pin, while MUX2 scans all pins and measures the voltage response.

The firmware repeats this process for all 8 injection channels and builds an 8x8 measurement matrix. After that, it calculates the average response for each injected row and selects the row with the lowest average voltage as the ground-pin candidate.

Key parameters:

| Parameter | Value | Description |
|---|---:|---|
| NUM_SAMPLES | 70 | Number of ADC samples per measurement |
| SETTLE_TIME | 60 ms | Delay after changing a MUX channel |
| SAMPLE_DELAY | 4 ms | Delay between samples |
| NUM_MEASUREMENT_PASSES | 2 | Number of repeated passes per matrix cell |

## Measurement and Filtering

To improve measurement stability, the firmware uses several filtering techniques:

- ADC warm-up conversions at startup.
- Dummy ADC readings after each MUX channel change.
- Multi-sample averaging in Mode 1.
- Large sample buffers in Mode 2.
- Sorting and trimming the top and bottom 15% of samples.
- Multi-pass averaging for each matrix cell.

These techniques reduce the effect of switching spikes, noise, and unstable readings.

## Ground Pin Detection

In Mode 2, the firmware calculates the average measured voltage for each injected row in the 8x8 matrix. The row with the lowest average response is selected as the most likely ground pin.

The selected row is then normalized between 0 and 1 for clearer display.

## How to Use

1. Connect the circuit according to the pin mapping table.
2. Flash the firmware to the ATmega32A.
3. Open a serial terminal.
4. Set the terminal to 9600 baud, 8N1.
5. Reset the microcontroller.
6. Wait for the UART menu to appear.
7. Send `1` to run the single MUX scan.
8. Send `2` to run the dual MUX matrix test.
9. Read the results from the terminal.
10. Press RESET to run another test.

## UART Menu

```text
=== IC Tester (Mode Select) ===
ATmega32A @ 11.059200 MHz
PC4 -> MUX1 INH (0=EN, 1=DIS)

Choose mode:
 1) Single MUX scan (MUX2->ADC0, MUX1 disabled)
 2) Dual MUX matrix test (MUX1 inject + MUX2 measure)

Enter 1 or 2:
```

## Configuration Notes

- The firmware assumes `VCC_MV = 5000`.
- If the actual AVCC is not exactly 5.000V, measure it and update the constant.
- If readings are unstable, increase the settling delays.
- Increasing the number of ADC samples improves stability but increases runtime.
- Make sure AVCC is connected and properly decoupled.
- Use a stable 5V supply.

## Validation Checklist

Before testing unknown ICs, verify the following:

- PC4 correctly controls the MUX1 inhibit pin.
- MUX1 injection is disabled at startup.
- ADC0 reads expected voltages from a known voltage source.
- PD2-PD7 correctly select the intended MUX channels.
- The IC socket pins match the MUX channel numbering.
- The 5V supply is stable.
- The UART terminal is configured correctly.

## Limitations

- Designed for 8-pin ICs only.
- Detects electrical signatures and ground-pin candidates, but does not fully identify IC functionality.
- Accuracy depends on wiring quality, supply stability, and IC socket mapping.
- Protection circuitry should be added before testing unknown or sensitive ICs.

## Future Improvements

- Add CSV logging on the connected PC.
- Add support for 14-pin and 16-pin ICs using additional multiplexers.
- Add a database for matching known IC signatures.
- Add protection circuitry such as current limiting and clamp diodes.
- Add a graphical PC interface for viewing the measurement matrix.

## Project Status

The project firmware and documentation are completed. The next step is uploading the source code, documentation, and supporting files to GitHub.

## Suggested Repository Structure

```text
IC-Tester-ATmega32A/
│
├── README.md
├── docs/
│   └── IC_Tester_Documentation.pdf
│
├── firmware/
│   └── main.c
│
├── images/
│   └── circuit_diagram.png
│
└── samples/
    └── uart_output.txt
```

## Author

Ahmed Negm

## Acknowledgment

This project was supervised by Dr. Hossam Eldin Mostafa Abdelbaki.
