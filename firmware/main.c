/*
 * main.c — MSP430FR6989 telemetry node (MSP-EXP430FR6989 LaunchPad)
 *
 * Minimal bring-up build. Every ~500 ms it:
 *   1. samples the internal temperature sensor (ADC12_B),
 *   2. streams one framed reading over the USB backchannel UART,
 *   3. toggles the on-board red LED (P1.0) as a heartbeat.
 *
 * No LCD, no crystal, no timers/LPM — everything runs off the internal DCO
 * (~1 MHz) in a plain polled loop. The goal here is a dead-simple, reliable
 * path from the chip to the serial port; the dashboard reads it on the host.
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

// --- Temperature-sensor factory calibration (TLV), 1.2 V reference ---
#define CALADC_15V_30C  (*((uint16_t *)0x1A1A))  // ADC @ 30 °C
#define CALADC_15V_85C  (*((uint16_t *)0x1A1C))  // ADC @ 85 °C

static uint16_t seq = 0;

// ---------------------------------------------------------------- clocks
// All internal, no crystal: the DCO (~1 MHz) drives MCLK and SMCLK (CPU + UART).
// ACLK isn't used by anything here. Keeping it crystal-free makes start-up
// deterministic — there's no oscillator-fault wait to hang on.
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

// ---------------------------------------------------------------- ADC temp
static void init_adc(void)
{
    // 1.2 V internal reference on for the temperature sensor.
    { uint16_t to = 0; while ((REFCTL0 & REFGENBUSY) && ++to) ; }  // bounded
    REFCTL0 = REFVSEL_0 | REFON;

    ADC12CTL0 = ADC12SHT0_8 | ADC12ON;      // long sample time, ADC on
    ADC12CTL1 = ADC12SHP;                   // pulse-mode sampling
    ADC12CTL2 = ADC12RES_2;                 // 12-bit resolution
    ADC12CTL3 = ADC12TCMAP;                 // map temp sensor to channel A30
    ADC12MCTL0 = ADC12VRSEL_1 | ADC12INCH_30; // VR+ = buffered VREF, temp channel

    __delay_cycles(400);                    // let the reference stabilise
}

static uint16_t adc_read_raw(void)
{
    ADC12CTL0 |= ADC12ENC | ADC12SC;        // start conversion
    uint16_t to = 0;
    while (!(ADC12IFGR0 & ADC12IFG0) && ++to) ;  // bounded: never hang if ADC stalls
    return ADC12MEM0;
}

// Convert the raw temp-sensor code to °C using the factory calibration.
static int read_temp_c_x10(void)
{
    int32_t raw = adc_read_raw();
    // degC = (raw - cal30) * (85 - 30) / (cal85 - cal30) + 30
    int32_t num = (raw - (int32_t)CALADC_15V_30C) * (85 - 30);
    int32_t den = (int32_t)CALADC_15V_85C - (int32_t)CALADC_15V_30C;
    int32_t c_x10 = (num * 10) / den + 300; // tenths of a degree
    return (int)c_x10;
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

// Build "$<seq>,TEMP,<value>*<CS>\r\n" and send it.
static void send_frame(int temp_c_x10)
{
    char payload[24];
    char *p = payload;
    int ip = temp_c_x10 / 10;
    int fp = temp_c_x10 % 10;
    if (fp < 0) fp = -fp;

    // payload = "<seq>,TEMP,<int>.<tenth>"
    p = put_u(p, seq);
    *p++ = ','; *p++ = 'T'; *p++ = 'E'; *p++ = 'M'; *p++ = 'P'; *p++ = ',';
    if (ip < 0) { *p++ = '-'; ip = -ip; }
    p = put_u(p, (unsigned)ip);
    *p++ = '.';
    *p++ = (char)('0' + fp);
    *p = '\0';

    // XOR checksum over the payload bytes.
    unsigned char cs = 0;
    const char *q;
    for (q = payload; *q; ++q) cs ^= (unsigned char)*q;

    static const char hex[] = "0123456789ABCDEF";
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

    // Polled loop: heartbeat, sample, send a frame, busy-wait ~500 ms.
    for (;;) {
        P1OUT ^= BIT0;                      // heartbeat (~1 Hz)
        send_frame(read_temp_c_x10());
        __delay_cycles(500000);             // ~0.5 s at ~1 MHz MCLK
    }
}
