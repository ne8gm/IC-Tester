/*
  ATmega32A Dual/Single 74HC4051 IC Tester - Mode Select on UART
  Clock: 11.059200 MHz - CodeVisionAVR

  Wiring (as your diagram):
  - MUX1 (Injection): S0..S2 -> PD2..PD4, COM -> +5V (VCC)
  - MUX2 (Measurement): S0..S2 -> PD5..PD7, COM -> ADC0 (PA0)
  - MUX1 INH (Enable/Disable): PC4
      INH = 0 -> Enabled  (inject active)
      INH = 1 -> Disabled (Hi-Z, no injection)

  At boot:
  - Prints menu:
      '1' Single MUX scan (MUX1 disabled)
      '2' Dual MUX matrix test (MUX1 enabled)
*/

#include <mega32a.h>
#include <delay.h>
#include <stdio.h>

// -------------------- CONFIG --------------------
#define VCC_MV 5000L   // put your measured AVCC in mV if needed (e.g. 4850)

// Single scan settings
#define SETTLE_MS_SINGLE        10
#define SAMPLE_DELAY_MS_SINGLE   2
#define AVG_SAMPLES_SINGLE      20

// Dual scan settings (your original)
#define NUM_SAMPLES 70
#define SETTLE_TIME 60
#define SAMPLE_DELAY 4
#define NUM_MEASUREMENT_PASSES 2

// -------------------- PINS --------------------
// MUX1 select pins (PD2..PD4)
#define MUX1_SEL_MASK 0x1C   // 00011100
// MUX2 select pins (PD5..PD7)
#define MUX2_SEL_MASK 0xE0   // 11100000

// MUX1 INH on PC4
#define MUX1_INH     PORTC.4
#define MUX1_INH_DDR DDRC.4

// -------------------- STORAGE (Dual) --------------------
float measurements[8][8];
float channelAverages[8];
float channelStdDev[8];
unsigned char groundPin;

// -------------------- LOW LEVEL ADC --------------------
static unsigned int adcReadOnce(void)
{
    ADCSRA |= 0x40;                 // ADSC start
    while (!(ADCSRA & 0x10));       // ADIF wait
    ADCSRA |= 0x10;                 // clear ADIF
    return ADCW;                    // 0..1023
}

static void adcDummy(unsigned char n)
{
    while (n--) {
        adcReadOnce();
    }
}

// -------------------- MUX CONTROL --------------------
static void mux1_enable(void)   { MUX1_INH = 0; }  // INH low = enable
static void mux1_disable(void)  { MUX1_INH = 1; }  // INH high = disable

static void setMux1Channel(unsigned char ch)
{
    ch &= 7;
    // PD2..PD4 = ch bits
    PORTD = (PORTD & ~MUX1_SEL_MASK) | (ch << 2);
    delay_ms(SETTLE_TIME);
    adcDummy(3);
}

static void setMux2Channel(unsigned char ch)
{
    ch &= 7;
    // PD5..PD7 = ch bits
    PORTD = (PORTD & ~MUX2_SEL_MASK) | (ch << 5);
    delay_ms(SETTLE_TIME);
    adcDummy(3);
}

// -------------------- HELPERS --------------------
static void sortFloatArray(float *arr, unsigned char size)
{
    unsigned char i, j;
    float temp;

    for (i = 0; i < size - 1; i++) {
        for (j = i + 1; j < size; j++) {
            if (arr[i] > arr[j]) {
                temp = arr[i];
                arr[i] = arr[j];
                arr[j] = temp;
            }
        }
    }
}

// -------------------- SINGLE MUX (measuring MUX2 only) --------------------
static unsigned int readAdcStable_Single(void)
{
    unsigned char i;
    unsigned long sum = 0;

    // flush old S/H
    adcReadOnce();
    adcReadOnce();

    for (i = 0; i < AVG_SAMPLES_SINGLE; i++) {
        sum += adcReadOnce();
        delay_ms(SAMPLE_DELAY_MS_SINGLE);
    }
    return (unsigned int)(sum / AVG_SAMPLES_SINGLE);
}

static void runSingleMuxScan(void)
{
    unsigned char ch;
    unsigned int raw;
    long mv;

    printf("\n=== MODE 1: Single MUX Scan (MUX2->ADC0) ===\n");
    printf("MUX1 injection is DISABLED (INH=1)\n\n");

    // ensure no injection
    mux1_disable();

    // faster settle for single scan (optional)
    for (ch = 0; ch < 8; ch++) {
        // Use shorter settle here instead of dual SETTLE_TIME:
        PORTD = (PORTD & ~MUX2_SEL_MASK) | (ch << 5);
        delay_ms(SETTLE_MS_SINGLE);
        adcDummy(2);

        raw = readAdcStable_Single();
        mv  = (long)raw * VCC_MV / 1023L;

        printf("CH=%d  PIN=%d  RAW=%u  mV=%ld\n", ch, ch, raw, mv);
        delay_ms(50);
    }

    printf("\nDone. Press RESET to run again.\n");
    while (1);
}

// -------------------- DUAL MUX (your original logic) --------------------
static float readVoltage(void)
{
    float readings[NUM_SAMPLES];
    unsigned int rawValue;
    unsigned char i;
    float sum = 0;
    unsigned char startIdx, endIdx, count;

    // discard first reading
    adcReadOnce();

    for (i = 0; i < NUM_SAMPLES; i++) {
        rawValue = adcReadOnce();
        readings[i] = (float)rawValue * (5.0 / 1023.0);
        delay_ms(SAMPLE_DELAY);
    }

    sortFloatArray(readings, NUM_SAMPLES);

    // remove top/bottom 15%
    startIdx = (NUM_SAMPLES * 15) / 100;
    endIdx   = NUM_SAMPLES - startIdx;

    sum = 0;
    count = 0;
    for (i = startIdx; i < endIdx; i++) {
        sum += readings[i];
        count++;
    }
    return sum / count;
}

static void performMeasurements(void)
{
    unsigned char mux1_ch, mux2_ch, pass;
    float sum, sumOfSquares, variance;
    float passReadings[NUM_MEASUREMENT_PASSES];
    unsigned char p;

    for (mux1_ch = 0; mux1_ch < 8; mux1_ch++) {
        setMux1Channel(mux1_ch);

        sum = 0;

        for (mux2_ch = 0; mux2_ch < 8; mux2_ch++) {
            setMux2Channel(mux2_ch);

            for (pass = 0; pass < NUM_MEASUREMENT_PASSES; pass++) {
                passReadings[pass] = readVoltage();
                if (pass < NUM_MEASUREMENT_PASSES - 1) delay_ms(10);
            }

            measurements[mux1_ch][mux2_ch] = 0;
            for (p = 0; p < NUM_MEASUREMENT_PASSES; p++) {
                measurements[mux1_ch][mux2_ch] += passReadings[p];
            }
            measurements[mux1_ch][mux2_ch] /= NUM_MEASUREMENT_PASSES;

            sum += measurements[mux1_ch][mux2_ch];
        }

        channelAverages[mux1_ch] = sum / 8.0;

        // std dev (kept as your logic)
        sumOfSquares = 0;
        for (mux2_ch = 0; mux2_ch < 8; mux2_ch++) {
            float diff = measurements[mux1_ch][mux2_ch] - channelAverages[mux1_ch];
            sumOfSquares += diff * diff;
        }
        variance = sumOfSquares / 8.0;
        channelStdDev[mux1_ch] = 0;

        if (variance > 0) {
            float x = variance, lastX;
            unsigned char iter;
            for (iter = 0; iter < 10; iter++) {
                lastX = x;
                x = (x + variance / x) / 2.0;
                if (x == lastX) break;
            }
            channelStdDev[mux1_ch] = x;
        }

        delay_ms(50);
    }
}

static void findAndDisplayGroundPin(void)
{
    unsigned char i;
    float lowestAvg;
    float minVoltage, maxVoltage, range;
    float normalizedReadings[8];

    // find ground pin (lowest average)
    groundPin = 0;
    lowestAvg = channelAverages[0];

    for (i = 1; i < 8; i++) {
        if (channelAverages[i] < lowestAvg) {
            lowestAvg = channelAverages[i];
            groundPin = i;
        }
    }

    // normalize based on row min/max (your original behavior)
    minVoltage = measurements[groundPin][0];
    maxVoltage = measurements[groundPin][0];

    for (i = 0; i < 8; i++) {
        if (measurements[groundPin][i] < minVoltage) minVoltage = measurements[groundPin][i];
        if (measurements[groundPin][i] > maxVoltage) maxVoltage = measurements[groundPin][i];
    }

    range = maxVoltage - minVoltage;

    if (range > 0.001) {
        for (i = 0; i < 8; i++) {
            normalizedReadings[i] = (measurements[groundPin][i] - minVoltage) / range;
        }
    } else {
        for (i = 0; i < 8; i++) normalizedReadings[i] = 0.0;
    }

    printf("\nGround Pin: %d\n\n", groundPin);
    for (i = 0; i < 8; i++) {
        printf("Pin %d: %.4f\n", i, normalizedReadings[i]);
    }
}

static void runDualMuxTest(void)
{
    printf("\n=== MODE 2: Dual MUX Matrix Test ===\n");
    printf("MUX1 injection is ENABLED (INH=0)\n\n");

    // enable injection
    mux1_enable();

    performMeasurements();
    findAndDisplayGroundPin();

    while (1);
}

// -------------------- MAIN --------------------
void main(void)
{
    unsigned char i;
    char mode = 0;

    // PORTD: PD2..PD7 outputs (both mux select lines)
    DDRD |= 0xFC;

    // PC4 as output for MUX1 INH
    MUX1_INH_DDR = 1;

    // Disable injection by default at boot (safe)
    mux1_disable();

    // ADC: AVCC ref, ADC0, right adjust
    ADMUX  = 0x40;

    // ADC enable, prescaler=64
    ADCSRA = 0x86;

    // UART 9600
    UBRRL = 71;
    UBRRH = 0;
    UCSRB = 0x18;
    UCSRC = 0x86;

    delay_ms(150);

    // warm-up ADC
    for (i = 0; i < 10; i++) {
        adcReadOnce();
        delay_ms(2);
    }

    printf("\n=== IC Tester (Mode Select) ===\n");
    printf("ATmega32A @ 11.059200 MHz\n");
    printf("PC4 -> MUX1 INH (0=EN, 1=DIS)\n\n");
    printf("Choose mode:\n");
    printf("  1) Single MUX scan (MUX2->ADC0, MUX1 disabled)\n");
    printf("  2) Dual MUX matrix test (MUX1 inject + MUX2 measure)\n");
    printf("Enter 1 or 2: ");

    // wait for user selection
    while (mode != '1' && mode != '2') {
        mode = getchar();
    }
    printf("%c\n", mode);

    if (mode == '1') {
        runSingleMuxScan();
    } else {
        runDualMuxTest();
    }

    while (1);
}
