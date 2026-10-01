/*
 * main.c — MSP430FR6989 telemetry node (MSP-EXP430FR6989 LaunchPad)
 *
 * Minimal bring-up build. Every ~500 ms it:
 *   1. samples the internal temperature sensor and the supply-voltage
 *      monitor (ADC12_B, AVCC/2 on channel A31),
 *   2. shows the rounded whole-degree temperature on the on-board segment LCD,
 *   3. streams a framed TEMP and VOLT reading over the USB backchannel UART,
 *   4. toggles the on-board red LED (P1.0) as a heartbeat.
 *
 * No crystal, no timers/LPM — everything runs off the internal DCO (~1 MHz) in
 * a plain polled loop, and the LCD is clocked from ACLK (= internal VLO), so
 * start-up stays deterministic with no oscillator to fault on.
 *
 * Frame format (see ../PROTOCOL.md):  $<seq>,TEMP,<value>*<CS>\r\n
 *
 * Backchannel UART is eUSCI_A1 on P3.4 (TXD) / P3.5 (RXD), 9600 8N1 — this is
 * the eZ-FET "Application UART1" that shows up as a COM port on the PC.
 *
 * Toolchain: TI Code Composer Studio (register-level, no driverlib).
 */
#include <msp430.h>
#include <stdint.h>
#include "hal_LCD.h"

// --- Temperature-sensor factory calibration (TLV), 1.2 V reference ---
#define CALADC_15V_30C  (*((uint16_t *)0x1A1A))  // ADC @ 30 °C
#define CALADC_15V_85C  (*((uint16_t *)0x1A1C))  // ADC @ 85 °C

static uint16_t seq = 0;

// ---------------------------------------------------------------- clocks
// All internal, no crystal: the DCO (~1 MHz) drives MCLK and SMCLK (CPU + UART),
// and the internal VLO (~9.4 kHz) drives ACLK, which clocks the LCD. Keeping it
// crystal-free makes start-up deterministic — no oscillator-fault wait to hang on.
static void init_clocks(void)
{
    CSCTL0_H = CSKEY_H;                     // unlock CS registers
    CSCTL1 = DCOFSEL_0;                     // DCO ~1 MHz
    CSCTL2 = SELA__VLOCLK | SELS__DCOCLK | SELM__DCOCLK;
    CSCTL3 = DIVA__1 | DIVS__1 | DIVM__1;   // no dividers
    CSCTL0_H = 0;                           // relock CS registers
}

// ---------------------------------------------------------------- UART A1
static void init_uart(void)
{
    P3SEL0 |= BIT4 | BIT5;                  // P3.4/P3.5 -> UCA1TXD/UCA1RXD
    P3SEL1 &= ~(BIT4 | BIT5);

    UCA1CTLW0 = UCSWRST;                    // hold in reset while configuring
    UCA1CTLW0 |= UCSSEL__SMCLK;            // SMCLK = ~1 MHz
    // 9600 baud @ 1 MHz, oversampling: BR=6, BRF=8, BRS=0x20, UCOS16=1
    UCA1BR0 = 6;
    UCA1BR1 = 0;
    UCA1MCTLW = 0x2000 | (8 << 4) | UCOS16;
    UCA1CTLW0 &= ~UCSWRST;                  // release for operation
}

static void uart_putc(char c)
{
    uint16_t to = 0;
    while (!(UCA1IFG & UCTXIFG) && ++to) ;  // bounded: never hang if TX stalls
    UCA1TXBUF = c;
}

static void uart_puts(const char *s)
{
    while (*s) uart_putc(*s++);
}

// ---------------------------------------------------------------- ADC
// Two internal channels that need different reference levels:
//   - temp sensor (A30): 1.2 V ref, to match the factory TLV calibration.
//   - supply monitor (A31 = AVCC/2): 2.5 V ref, since AVCC/2 (~1.65 V) is above
//     1.2 V and would saturate. So we pick the reference level per reading.
static void init_adc(void)
{
    ADC12CTL0 = ADC12SHT0_8 | ADC12ON;      // long sample time, ADC on
    ADC12CTL1 = ADC12SHP;                   // pulse-mode sampling
    ADC12CTL2 = ADC12RES_2;                 // 12-bit resolution
    ADC12CTL3 = ADC12TCMAP | ADC12BATMAP;   // map temp->A30, AVCC/2->A31
}

// Select the internal reference level (REFVSEL_0=1.2 V, _1=2.0 V, _2=2.5 V) and
// let it settle before the next conversion.
static void ref_use(uint16_t refvsel)
{
    uint16_t to = 0;
    while ((REFCTL0 & REFGENBUSY) && ++to) ;    // bounded
    REFCTL0 = refvsel | REFON;
    __delay_cycles(1000);                        // ~1 ms: let the reference settle
}

// One 12-bit conversion on the given input channel; buffered VREF as VR+.
static uint16_t adc_convert(uint16_t inch)
{
    uint16_t to = 0;
    ADC12CTL0 &= ~ADC12ENC;                 // allow the MCTL0 channel change
    ADC12MCTL0 = ADC12VRSEL_1 | inch;       // VR+ = buffered VREF, VR- = AVSS
    ADC12CTL0 |= ADC12ENC | ADC12SC;        // start conversion
    while (!(ADC12IFGR0 & ADC12IFG0) && ++to) ;  // bounded: never hang if ADC stalls
    return ADC12MEM0;
}

// Temperature in tenths of a degree C, via the factory TLV calibration.
static int read_temp_c_x10(void)
{
    int32_t raw, num, den, c_x10;
    ref_use(REFVSEL_0);                     // 1.2 V ref matches the TLV cal
    raw = adc_convert(ADC12INCH_30);
    // degC = (raw - cal30) * (85 - 30) / (cal85 - cal30) + 30
    num = (raw - (int32_t)CALADC_15V_30C) * (85 - 30);
    den = (int32_t)CALADC_15V_85C - (int32_t)CALADC_15V_30C;
    c_x10 = (num * 10) / den + 300;         // tenths of a degree
    return (int)c_x10;
}

// Supply voltage in hundredths of a volt. A31 reads AVCC/2; with the 2.5 V
// reference, Vcc = 2 * (raw/4095) * 2.5 V = raw * 500 / 4095 (in x100 V).
static int read_vcc_x100(void)
{
    int32_t raw;
    ref_use(REFVSEL_2);                     // 2.5 V ref (AVCC/2 ~1.65 V fits)
    raw = adc_convert(ADC12INCH_31);
    return (int)((raw * 500) / 4095);
}

// ---------------------------------------------------------------- framing
// Hand-rolled integer -> ASCII. We avoid sprintf/printf deliberately: the TI
// runtime's printf is large and very stack-hungry, which overflows the small
// default stack on this part and traps the program out to exit(). This is a
// couple dozen bytes of stack and never touches the heap.
static char *put_u(char *p, unsigned v)
{
    char tmp[5];                            // 65535 -> 5 digits max
    int n = 0;
    do { tmp[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (n) *p++ = tmp[--n];
    return p;
}

// Build "$<seq>,<TAG>,<value>*<CS>\r\n" and send it. `value` is scaled by
// 10^decimals (decimals is 1 or 2), e.g. 261 with decimals=1 -> "26.1",
// 330 with decimals=2 -> "3.30".
static void send_reading(const char *tag, int value, int decimals)
{
    char payload[28];
    char *p = payload;
    int scale = (decimals == 2) ? 100 : 10;
    int neg = value < 0;
    int av = neg ? -value : value;
    int ip = av / scale;
    int fp = av % scale;
    const char *q;
    unsigned char cs = 0;
    static const char hex[] = "0123456789ABCDEF";

    // payload = "<seq>,<TAG>,<int>.<frac>"
    p = put_u(p, seq);
    *p++ = ',';
    while (*tag) *p++ = (char)*tag++;
    *p++ = ',';
    if (neg) *p++ = '-';
    p = put_u(p, (unsigned)ip);
    *p++ = '.';
    if (decimals == 2) { *p++ = (char)('0' + fp / 10); *p++ = (char)('0' + fp % 10); }
    else               { *p++ = (char)('0' + fp); }
    *p = '\0';

    // XOR checksum over the payload bytes.
    for (q = payload; *q; ++q) cs ^= (unsigned char)*q;

    uart_putc('$');
    uart_puts(payload);
    uart_putc('*');
    uart_putc(hex[(cs >> 4) & 0xF]);
    uart_putc(hex[cs & 0xF]);
    uart_putc('\r');
    uart_putc('\n');
    seq++;
}

// ---------------------------------------------------------------- main
int main(void)
{
    int i;

    WDTCTL = WDTPW | WDTHOLD;               // stop watchdog
    PM5CTL0 &= ~LOCKLPM5;                   // unlock GPIO after FRAM boot

    // Boot blink: flash the red LED 3x before any peripheral setup. If you see
    // this at power-on, the chip is running our code; the steady heartbeat
    // below then confirms it reached the main loop.
    P1DIR |= BIT0;
    P1OUT &= ~BIT0;
    for (i = 0; i < 6; i++) {
        P1OUT ^= BIT0;
        __delay_cycles(120000);            // ~0.12 s at the ~1 MHz reset DCO
    }
    P1OUT &= ~BIT0;

    init_clocks();
    init_uart();
    init_adc();
    LCD_init();
    LCD_clear();

    // Polled loop: heartbeat, sample temp + supply voltage, update LCD,
    // stream a frame for each, then wait ~500 ms.
    for (;;) {
        int t_x10, v_x100;
        P1OUT ^= BIT0;                      // heartbeat (~1 Hz)
        t_x10 = read_temp_c_x10();
        v_x100 = read_vcc_x100();
        LCD_showInt((t_x10 + 5) / 10);      // rounded whole degrees
        send_reading("TEMP", t_x10, 1);
        send_reading("VOLT", v_x100, 2);
        __delay_cycles(500000);             // ~0.5 s at ~1 MHz MCLK
    }
}
